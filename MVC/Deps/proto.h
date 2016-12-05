#define P_ADD		'A'
#define P_UPDATE	'U'
#define P_DELETE	'D'
#define P_MANY		'M'
#define P_NOTHING	'N'

#define MAX_TRANSMISSION_SIZE 1000
#define RECV_PORT "5003"
#define SEND_PORT "5004"

struct KVP
{
	char *key;
	char *value;
};

struct ReqRes
{
	char type;
	int  number;
	char *user;
	struct KVP *attrbs;
};

void connListener(void *nparam);
struct ReqRes *gobble_response(char *rawr, struct ReqRes* rr);
void emit_request(struct ReqRes *rr, char *bucket);
void getKVP(char *line, struct KVP *bucket, char sep);
char *getLine(char **buffer, int update);

struct ReqRes *gobble_response(char *rawr, struct ReqRes *rr)
{

	char *line;
	int lineno=0, ano=0;

	rr->number = 0;
	rr->type = P_NOTHING;

	while((line = getLine(&rawr, 1)) != NULL)
	{
		
		switch(lineno)
		{
		case 0:
			rr->user = line;
			break;
		/* Type of Request/Responce */
		case 1:
			rr->type = line[0];
			break;
		/* Number of elements */
		case 2:
			rr->number = atoi(line);
			rr->attrbs = (struct KVP*)malloc(rr->number * sizeof(struct KVP));
			break;
		/* All KVPs */
		default:
			getKVP(line, &rr->attrbs[ano++], ':');
		}
		lineno++;
	}
	return rr;
}

void emit_request(struct ReqRes *rr, char *bucket)
{
	int i;

	memset(bucket, 0, 1000);

	bucket += 
		sprintf(bucket, 
			"%s\r\n"
			"%c\r\n"
			"%d\r\n",
			rr->user,
			rr->type,
			rr->number);

	for(i=0; i<rr->number; i++) {
		bucket += 
			sprintf(bucket,
				"%s : %s\r\n",
				rr->attrbs[i].key,
				rr->attrbs[i].value
				);
	}
}

void getKVP(char *line, struct KVP *bucket, char sep)
{
	while(*line && (*line == ' ' || *line == '\t')) line++;
	bucket->key = line;

	while(*line && *line != sep) line++;
	*line++ = '\0';

	while(*line && (*line == ' ' || *line == '\t')) line++;
	bucket->value = line;
}

char *getLine(char **buffer, int update)
{
	char *tmp = *buffer, *start = *buffer;
	while(*tmp && *tmp != '\n')
		tmp++;
	*tmp = '\0';
	if(update)
		*buffer = tmp+1;

	if(*start == '\0')
		return NULL;
	return start;
}

