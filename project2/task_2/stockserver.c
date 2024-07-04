
#include "csapp.h"
#include <time.h>

typedef struct {
int *buf; /* Buffer array */
int n; /* Maximum number of slots */
int front; /* buf[(front+1)%n] is first item */
int rear; /* buf[rear%n] is last item */
sem_t mutex; /* Protects accesses to buf */
sem_t slots; /* Counts available slots */
sem_t items; /* Counts available items */
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n)
{
sp->buf = Calloc(n, sizeof(int));
sp->n = n; /* Buffer holds max of n items */
sp->front = sp->rear = 0; /* Empty buffer iff front == rear */
Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
Sem_init(&sp->items, 0, 0); /* Initially, buf has 0 items */
}
void sbuf_deinit(sbuf_t *sp)
{
Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{
P(&sp->slots); /* Wait for available slot */
P(&sp->mutex); /* Lock the buffer */
sp->buf[(++sp->rear)%(sp->n)] = item; /* Insert the item */
V(&sp->mutex); /* Unlock the buffer */
V(&sp->items); /* Announce available item */
}
int sbuf_remove(sbuf_t *sp)
{
int item;
P(&sp->items); /* Wait for available item */
P(&sp->mutex); /* Lock the buffer */
item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
V(&sp->mutex); /* Unlock the buffer */
V(&sp->slots); /* Announce available slot */
return item;
}

static int SBUFSIZE = 120;

sbuf_t sbuf;

static int NTHREADS = 120;

static int byte_cnt = 0;
static sem_t mutex;

static void init_handle_cnt(void);
void handleStocks(int connfd);
void *thread(void *vargp);
int checkBuf(char* buf);

typedef struct {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t w;
} stock_item;

/** binary tree start  **/

typedef struct node{
    stock_item* stock;
    struct node* left;
    struct node* right;
} node;

node* root;

node* insert(node* root, stock_item* s){
    if(root == NULL) {
        root = (node*)malloc(sizeof(node));
        root->stock = s;
        root->left = NULL;
        root->right = NULL;
    } else {
        if (s->ID < root->stock->ID){
            root->left = insert(root->left, s);
        } else {
            root->right = insert(root->right, s);
        }
    }
    return root;
}

node* searchById(node* root, int id){
    if(root == NULL) {
        return NULL;
    }
    if(root->stock->ID == id){
        return root;
    }else{
        if(root->stock->ID > id){
            return searchById(root->left, id);
        }
        else{
            return searchById(root->right, id);
        }
    }
}

void pre(node* n){
    if (n) {
        printf("%d %d %d\n", n->stock->ID, n->stock->left_stock, n->stock->price);
        pre(n->left);
        pre(n->right);
    }
}

void writeFile(node* n, FILE* fp){
    if (n) {
        fprintf(fp, "%d %d %d\n", n->stock->ID, n->stock->left_stock, n->stock->price);
        pre(n->left);
        pre(n->right);
    }
}

void getAllNode(node* root, node** nodes, int* cnt){
    if(root == NULL){
        return ;
    }
    nodes[*cnt] = root;
    (*cnt)++;
    getAllNode(root->left, nodes, cnt);
    getAllNode(root->right, nodes, cnt);
}

/** binary tree end **/

void sighandler(int sig){
    node* nodes[101];
    int cnt = 0;
    getAllNode(root, nodes, &cnt);
    FILE* fp2 = fopen("stock.txt", "w");
    for(int i=0;i<cnt;i++){
        fprintf(fp2 ,"%d %d %d\n", nodes[i]->stock->ID, nodes[i]->stock->left_stock, nodes[i]->stock->price);
    }
    fclose(fp2);
    exit(0);
}

int stock_cnt;



int main(int argc, char **argv) 
{
    /* start init stocks */
    signal(SIGINT, sighandler);
    
    int id, left, price;
    char * buffer[MAXLINE];
    
    FILE* fp = fopen("stock.txt", "r");
    if(fp == NULL) {
        printf("file open error\n");
        return 1;
    }
    
    stock_cnt = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL){
        if (sscanf(buffer, "%d %d %d", &id, &left, &price) == 3) {
            stock_item* s = (stock_item*)malloc(sizeof(stock_item));
            s->ID = id;
            s->left_stock = left;
            s->price = price;
            s->readcnt = 0;
            Sem_init(&(s->w), 0, 1);
            Sem_init(&(s->mutex), 0, 1);
            root = insert(root, s);
            stock_cnt += 1;
        }
    }
    fclose(fp);
    /*end init stocks*/
    
    
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    
    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }
    
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
    }
    
    /* table save */
    node* nodes[101];
    int cnt = 0;
    getAllNode(root, nodes, &cnt);
    FILE* fp2 = fopen("stock.txt", "w");
    for(int i=0;i<cnt;i++){
        fprintf(fp2 ,"%d %d %d\n", nodes[i]->stock->ID, nodes[i]->stock->left_stock, nodes[i]->stock->price);
    }
    fclose(fp2);
    exit(0);
}
/* $end echoserverimain */

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        handleStocks(connfd);
        Close(connfd);
    }
}

static void init_handle_cnt(void){
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void handleStocks(int connfd){
    int n;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    
    Pthread_once(&once, init_handle_cnt);
    Rio_readinitb(&rio, connfd);
    
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {

        byte_cnt += n;
        printf("thread %d received %d (%d total) bytes on fd %d\n",(int) pthread_self(), n, byte_cnt, connfd);
        
        
        char inst[MAXLINE];
        strcpy(inst, buf);
        inst[strlen(inst)-1] = '\0';
        printf("%s\n", inst);
        int option = checkBuf(inst);
        
        node* nodes[101];
        int cnt = 0;
        getAllNode(root, nodes, &cnt);
        
        
        if (option == 0){
            strcpy(buf, "show\n");
            buf[strlen(buf)] = '\0';
            
            for(int i=0;i<stock_cnt;i++){
                P(&nodes[i]->stock->mutex);
                nodes[i]->stock->readcnt++;
                if (nodes[i]->stock->readcnt == 1) {
                    P(&nodes[i]->stock->w);
                }
                V(&nodes[i]->stock->mutex);
                
                char tmp[100];
                
                sprintf(tmp ,"%d %d %d\n", nodes[i]->stock->ID, nodes[i]->stock->left_stock, nodes[i]->stock->price);
                strcat(buf, tmp);
                buf[strlen(buf)]='\0';
                P(&nodes[i]->stock->mutex);

                nodes[i]->stock->readcnt--;
                if (nodes[i]->stock->readcnt == 0){
                    V(&nodes[i]->stock->w);
                }
                V(&nodes[i]->stock->mutex);
            }
            Rio_writen(connfd, buf, MAXLINE);
        }
        else if(option == 1){
            buf[strlen(buf)] = '\0';
            char* token = strtok(NULL, " ");
            int id = atoi(token);
            token = strtok(NULL, " ");
            int howMany = atoi(token);
            node* n = searchById(root, id);
            
            if(n!=NULL){
                
                int left = n->stock->left_stock - howMany;
                if(left < 0) {
                    strcat(buf, "Not enough left stock\n");
                }else {
                    P(&(n->stock->w));
                    n->stock->left_stock = left;
                    strcat(buf, "[buy] success\n");
                    V(&(n->stock->w));
                }
                
            }else{
                strcat(buf, "id no exists.\n");
            }
            Rio_writen(connfd, buf, MAXLINE);
        } else if(option == 2){
            buf[strlen(buf)] = '\0';
            char* token = strtok(NULL, " ");
            int id = atoi(token);
            token = strtok(NULL, " ");
            int howMany = atoi(token);
            node* n = searchById(root, id);
            
            if(n!=NULL){
                P(&(n->stock->w));
                int left = n->stock->left_stock + howMany;
                n->stock->left_stock = left;
                strcat(buf, "[sell] success\n");
                V(&(n->stock->w));
            }else{
                strcat(buf, "id no exists.\n");
            }
            Rio_writen(connfd, buf, MAXLINE);
        } else if(option == 3){
            buf[strlen(buf)] = '\0';
            strcat(buf, "disconnection with server\n");
            Rio_writen(connfd, buf, MAXLINE);
            break;
        } else if(option == 4){
            strcpy(buf, "");
            Rio_writen(connfd, buf, MAXLINE);
        }
    }
}


int checkBuf(char* buf){
    if(!strcmp(buf, "show")){
        return 0;
    }else if(!strcmp(buf, "exit")){
        return 3;
    }

    char* token = strtok(buf, " ");
    if(!strcmp(token, "buy")){
        return 1;
    }else if(!strcmp(token, "sell")){
        return 2;
    }
    return 4;
}
