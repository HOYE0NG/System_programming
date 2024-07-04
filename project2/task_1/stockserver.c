/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready; // fd to handle
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;
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

int byte_cnt = 0;
int stock_cnt;


void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
int checkBuf(char* buf);

// for measurement


int main(int argc, char **argv){
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
    static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    
    init_pool(listenfd, &pool);
    
    
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
        
        
        if(FD_ISSET(listenfd, &pool.ready_set)) {

            
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }
        
        check_clients(&pool);
        
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


void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for(i=0; i<FD_SETSIZE; i++){
        p->clientfd[i] = -1;
    }
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for(i=0; i<FD_SETSIZE; i++){
        if(p->clientfd[i] < 0){
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);
            
            if(connfd > p->maxfd){
                p->maxfd = connfd;
            }
            if(i > p->maxi){
                p->maxi = i;
            }
            break;
        }
    }
    if(i == FD_SETSIZE){
        app_error("add_client error: Too many clients");
    }
}

void check_clients(pool *p){

    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; (i<= p->maxi) && (p->nready>0); i++){
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if((connfd >0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;

            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
               
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                
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
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    
                } else if(option == 4){
                    strcpy(buf, "");
                    Rio_writen(connfd, buf, MAXLINE);
                }
            }
            else{
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
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

