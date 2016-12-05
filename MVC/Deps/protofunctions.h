/*
 * This file is highly cohesive if you define
 * these variables and functions
 *
 * variables: 
 *	1.  char *user_name;	//Name of the application [aka user]
 *
 * functions:
 *	1.  void set_message(char *);	//method to show messages
 *
 */

#include "proto.h"
#include "connector.h"
#include "obcoll.h"
#include <Windows.h>

int connected = 0;

void build_request(char type, int nAttrbs,
	struct KVP *attrbs, char *obucket)
{
	ReqRes rr;

	rr.user = user_name;
	rr.type = type;
	rr.number = nAttrbs;
	rr.attrbs = attrbs;

	emit_request(&rr, obucket);
}

void add_new_item()
{

	int d;
	char tbuffer[100], rbuffer[1000];
	ReqRes rr;
	Connection *c = new Connection();

	/* HACKS: if this check is not done anyone can send the updates. */
	if (!connected) {
		set_message("You are not connected.");
		return;
	}

	rr.attrbs = (struct KVP*)malloc(sizeof(struct KVP));

	/*
	 * always generate a number greater than 20 and less than 100,
	 * so that we can see it on graph.
	 */
	d = rand() % 80 + 20;

	/* generate a key vaue pair */
	sprintf(tbuffer, "Item %d", d);
	rr.attrbs->key = new_cstring(tbuffer);
	sprintf(tbuffer, "%d", d);
	rr.attrbs->value = new_cstring(tbuffer);

	build_request(P_ADD, 1, rr.attrbs, rbuffer);

	c->Connect("localhost", SEND_PORT);
	c->Send(rbuffer, 1000);
	c->Close();

}

/*
 * triggered when user clicks connect button
 */
void connect_to_model(void *tparam)
{

	int x;
	char buffer[1001], *coll_updates, *coll_vachana;
	struct ReqRes rr;
	Connection *listener = new Connection();
	ChartData *graph_data = (ChartData *)tparam;

	if (!connected || (listener->Send("ping", 5) == 0)) {

		/* not connected try to connect. */
		if (listener->Connect("localhost", RECV_PORT)) {

			listener->Send(user_name, 100);

			/*
			 * check there is any message for you back.
			 * if there is something then you are surely not
			 * connected. Display that error message
			 */
			listener->Recieve(buffer, MAX_TRANSMISSION_SIZE);
			buffer[MAX_TRANSMISSION_SIZE] = '\0';

			if (buffer[0]) {
				set_message(buffer);
				ExitThread(0);
			}

			connected = 1;
			set_message("Connected to Model");
		}
		else {

			set_message("Connection to Model failed");
			return;
		}
	}
	else {
		/* you are already connected. */
		return;
	}

	do {

		listener->Recieve(buffer, MAX_TRANSMISSION_SIZE);
		buffer[MAX_TRANSMISSION_SIZE] = '\0';

		if (!strcmp(buffer, "ping")) {
			/* it was just a ping, continue listening */
			continue;
		}

		/* eat the response */
		gobble_response(buffer, &rr);

		coll_updates = rr.number > 1 ? "some" : "an";
		coll_vachana = rr.number > 1 ? "s" : "";

		switch (rr.type)
		{
		case P_ADD:
		case P_MANY:
			for (x = 0; x < rr.number; x++) {

				graph_data->add_item(
					new_CDATA_(
						rr.attrbs[x].key,
						atoi(rr.attrbs[x].value),
						getRandColor()
					)
				);
			}
			sprintf(
				buffer, "%s added %s Item%s.",
				rr.user, coll_updates, coll_vachana
			);
			set_message(buffer);
			break;

		case P_DELETE:
			/* not implemented */
			break;

		case P_UPDATE:
			graph_data->modify_item(
				atoi(rr.attrbs[0].key),
				atoi(rr.attrbs[0].value)
			);

			sprintf(
				buffer, "%s sent %s update%s.",
				rr.user, coll_updates, coll_vachana
			);
			set_message(buffer);
			break;

		case P_NOTHING:
			/* not implemented [not of use at all.]*/
			break;
		}

	} while (1);
}

void send_update(int index)
{

	char tbuffer[10], rbuffer[1000];
	struct KVP kvp;
	Connection *c = new Connection();


	if (index == -1) {
		/* nothing is selected */
		return;
	}

	sprintf(tbuffer, "%d", index);
	kvp.key = new_cstring(tbuffer);
	sprintf(tbuffer, "%d", rand() % 50 + 30);
	kvp.value = new_cstring(tbuffer);

	build_request(P_UPDATE, 1, &kvp, rbuffer);

	if (c->Connect("localhost", SEND_PORT)) {
		c->Send(rbuffer, MAX_TRANSMISSION_SIZE);
	}
	else {
		set_message("sending update failed");
	}
}
