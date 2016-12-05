#include "listener.h"
#include "connector.h"
#include "proto.h"
#include "dl_list.h"
#include "thelper.h"
#include "chartdata.h"

#define MAX_TRANSMISSION_SIZE 1000
#define MAX_CLIENTS 10

#define mlock()			{ while(lock); lock = 1; }
#define munlock()		{ lock = 0; int locktime = 30; while(--locktime); }

struct MyConnection
{
	char *name;
	Connection *c;
};

int
	/* port to recieve updates. [for client.] */
	BROADCAST_PORT=5003,

	/* port to send updates [for client.] */
	UPDATES_PORT=5004
	;	

/* the connections to clients where we send the updates. */
struct MyConnection *clients[MAX_CLIENTS];

/* the dataSource where we store the updates. */
ChartData *graph_data;

/* the variable used to attain mutex between threads. */
int lock = 0;

void get_clients(void *x);
void send_old_data(void *index);


struct MyConnection *new_MyConnection(Connection *c, char *client_name) {
	struct MyConnection *m = ((struct MyConnection*)malloc(sizeof(struct MyConnection)));
	if (m) {
		m->c = c;
		m->name = new_cstring(client_name);
	}
	return m;
}

void wait_n_exit() {
	printf("Exiting ....");
	getchar();
	exit(0);
}

int main(void) 
{
	int i;
	ReqRes rr;
	Connection *c = NULL;
	Listener *l = new Listener();
	char buffer[MAX_TRANSMISSION_SIZE + 1], altbuf[MAX_TRANSMISSION_SIZE + 1];

	for (i = 0; i < MAX_CLIENTS; i++) {
		clients[i] = NULL;
	}
	graph_data = new ChartData();

	/* create threads for getting clients */
	tcreate(get_clients, NULL);

	if (l->Bind(UPDATES_PORT) == -1) {
		puts("Binding failed on UPDATES_PORT");
		wait_n_exit();
	}

	puts("Ready to accept updates");

	while(1) {

		/* accept a connection. */
		c = l->Accept();

		if (c == NULL) {
			puts("Failed to accept connection. Exiting...");
			wait_n_exit();
		}

		/* recieve the updates by client in the buffer. */
		c->Recieve(buffer, MAX_TRANSMISSION_SIZE);
		
		buffer[MAX_TRANSMISSION_SIZE] = '\0';
		puts("Update recieved");

		/* after we gobble the resopnse the buffer gets modified hence store a copy */
		memcpy(altbuf, buffer, MAX_TRANSMISSION_SIZE);

		/* read the response and build the ReqRes object. */
		gobble_response(altbuf, &rr);

		/* If the update has no meaning close the connection and leave it. */
		if (rr.number <= 0) {
			goto send_out;
		}

		/* acquire lock on the datastructure. */
		mlock();

		/*
		 * iterate through all the update list and verify they are valid.
		 *
		 * valid => value > 0
		 */
		for (i = 0; i < rr.number; i++) {

			/* value less than 1, the whole update list is invalid */
			if (atoi(rr.attrbs[i].value) < 1) {

				/* release lock and loop back for accepting connections */
				munlock();
				goto send_out;
			}

			/* if update is of add type then add the update to graphData. */
			if ((int)rr.type == P_ADD) {

				graph_data->add_item(
					new_CDATA_(
						rr.attrbs[i].key,
						atoi(rr.attrbs[i].value),
						0
					)
				);

			}
			/* else if it's a update to older data then update it.*/
			else if ((int)rr.type == P_UPDATE) {

				graph_data->modify_item(
					atoi(rr.attrbs[i].key),
					atoi(rr.attrbs[i].value)
				);
			}
		}

		/* release the lock. */
		munlock();

		/*
		 * send it to all your clients
		 *
		 * TODO:
		 *	Could have created threads for sending the data to all clients.
		 */
		for (i = 0; i < MAX_CLIENTS; i++) {

			if (clients[i] != NULL) {

				if (clients[i]->c->Send(buffer, MAX_TRANSMISSION_SIZE) == 0) {

					printf("%s got disconnected\n", clients[i]->name);
					free(clients[i]);
					clients[i] = NULL;
				}
			}
		}
send_out:
		c->Close();
	}
	
	return 0;
}


void get_clients(void *x)
{
	int i;
	Connection *c;
	char client_name[100];
	Listener *l = new Listener();

	puts("Ready to accept clients");
	
	if (l->Bind(BROADCAST_PORT) == -1) {
		puts("Binding failed on BROADCAST_PORT.");
		wait_n_exit();
	}

	/* accept connections and add them to list */	
	while(1) {
		
		c = l->Accept();

		if (c == NULL) {
			puts("Accepting new client connections failed");
			wait_n_exit();
		}

		memset(client_name, 0, 100);
		if (c->Recieve(client_name, 100) < 1) {
			continue;
		}

		/*
		 * if it's a ping skip it.
		 * but don't close the connection ping is done on 
		 * the same connection which was attained before.
		 */
		if (!strcmp(client_name, "ping")) {
			continue;
		}

		/* iterate through the connections if some space found then insert new client */
		for (i = 0; i < MAX_CLIENTS; i++) {

			if (clients[i] == NULL) {

				clients[i] = new_MyConnection(c, client_name);
				printf("Client [%s] got connected\n", client_name);

				if (graph_data->count() > 0) {

					/* send the previous data stored to new client */
					tcreate(send_old_data, (void*)i);
				}

				break;
			}
			else
			{
				/*
				 * check whether he is alive if he is not close the connection
				 */
				if (clients[i]->c->Send("ping", 5) == 0) {

					clients[i] = NULL;

					/* let the for-loop see this client once again. */
					i--;
				}
			}
		}

		if (i == MAX_CLIENTS) {
			
			/*
			 * no place for new client, send error message to  client
			 * so that he understands service not available.
			 */
			c->Send("Sorry, service not available", 30);
			c->Close();
		}
		else {
			/* send a null string he will understand : no errors */
			c->Send("", 1);
		}
	}
}

void send_old_data(void *index)
{
	int i = 0;
	void *data;
	struct ReqRes rr;
	char buffer[MAX_TRANSMISSION_SIZE + 1], ibuf[10];

	printf("Flushing off data to %s as Server\n", clients[(int)index]->name);
	puts("waiting to acquire lock on Data");

	while (lock);
	lock = 1;

	puts("locked Data");

	rr.attrbs = (struct KVP*)malloc(sizeof(struct KVP) * graph_data->count());
	rr.user = "Server";
	rr.number = graph_data->count();
	rr.type = P_MANY;

	foreach_coll(data, graph_data) {

		sprintf(ibuf, "%d", BC_PORT(data)->value);
		rr.attrbs[i].value = new_cstring(ibuf);
		rr.attrbs[i].key = BC_PORT(data)->desc;
		i++;
	}

	lock = 10;
	while (--lock);

	emit_request(&rr, buffer);
	clients[(int)index]->c->Send(buffer, MAX_TRANSMISSION_SIZE);

	puts("Done sending them");
	ExitThread(0);
}
