/* cliente.c
   Elias P. Duarte Jr. */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "Packet.h"
#include <pthread.h>


void prepara_msg(Packet *msg,char *buffer,char *host);
void init_fila (Fila *fila);
void push_fila(Fila *fila, Packet *msg);
Packet *pop_fila(Fila *fila);
unsigned crc8(unsigned crc, char *data, size_t len);
void copia_string (char *entrada, char *copia);
double timestamp(void);

Fila *fila;
char host[]={'p','r','i','o','r','a','t', 0, 0, 0} ;
char destiny[] ={'b','o','w','m','o','r','e', 0, 0, 0}  ;
struct sockaddr_in sa, isa;
int sockdescr, socketserver;

void* escuta()
{
    Packet *msg = malloc(sizeof(Packet));
    Packet *aux = malloc(sizeof(Packet));
    double tempo_inicial;
    unsigned int i;
    int a;
    struct timeval read_timeout;
    char *c;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(socketserver, &readset);
	printf("Antes recv\n");
	while(1){
        recvfrom(socketserver, msg, 280, 0, (struct sockaddr *) &isa, sizeof(isa));
        //Decodificar CRC
        printf("recebeu\n");
	c = (char *)msg;
        if (crc8(0,c,279) == msg->crc){ // Se crc calculado da msg é igual ao campo crc da msg ele executa, caso contrario ele nao repassa a msg
            printf("primeiro crc\n");
            if(msg->token == 1){ // Ve se é o token
            printf("token\n");
                msg->token = 0;
                msg->monitor = 1;
                printf("source: %s\ndestiny: %s\n",msg->source, msg->destiny);
                c = (char *)msg;
                msg->crc = crc8(0, c, 279);
                sendto(sockdescr, msg, 280, 0, (struct sockaddr *) &sa, sizeof(sa));
                read_timeout.tv_sec = 5;
                read_timeout.tv_usec = 5000;
                a = 100;
                a = select(socketserver+1,&readset,NULL,NULL,&read_timeout);
                printf("Resposta do select: %d\n", a);
                if (a != 1) { // A confirmação do token está de boas, caso nao esteja ele volta a escutar
                    if ((fila->tam > 0) && (msg->hi_priority <= fila->hi_priority)) { // Tem msg pra enviar e prioridade da fila maior ou igual a prioridade da rede
                        aux = msg;
                        tempo_inicial = timestamp();
                        while ((((timestamp() - tempo_inicial)/1000) < 10) && (fila->hi_priority >= aux->hi_priority) && (fila->tam !=0)) {// While de tempo de bastão enviando mensagem
                            Packet *prox = pop_fila(fila);
                            while(1){ // timeout de envio de msg
                                sendto(sockdescr, prox, 280, 0, (struct sockaddr *) &sa, sizeof(sa));
                                read_timeout.tv_sec = 2;
                                read_timeout.tv_usec = 2000;
                                a = 100;
                                a = select(socketserver+1,&readset,NULL,NULL,&read_timeout);
                                if (a == 1) { //Mensagem foi recebida
                                    recvfrom(socketserver, aux, 280, 0, (struct sockaddr *) &isa, sizeof(isa));
                                    if ((strcmp(aux->destiny, prox->destiny) == 0) && (strcmp(aux->source, prox->source) == 0) && (aux->monitor == 1)) { //Mensagem recebida pelo destino
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    // Mandar token pra rede
                    printf("gerando token\n");
                    msg->token = 1;
                    msg->monitor = 0;
                    copia_string(host, msg->source);
                    copia_string(destiny, msg->destiny);
                    c = (char *)msg;
                    msg->crc = crc8(0, c, 279);
                    while(1){ // timeout de envio de token
                        printf("while de timout\n");
                        sendto(sockdescr, msg, 280, 0, (struct sockaddr *) &sa, sizeof(sa));
                        read_timeout.tv_sec = 5;
                        read_timeout.tv_usec = 5000;
                        a = 100;
                        a = select(socketserver+1,&readset,NULL,NULL,&read_timeout);
                        printf("a: %d\n", a);
                        if (a == 1) { //Mensagem foi recebida
                            recvfrom(socketserver, aux, 280, 0, (struct sockaddr *) &isa, sizeof(isa));
                            printf("Lendo a confirmação: %s\ndestiny: %s\n",aux->source, aux->destiny);
                            if ((strcmp(aux->destiny, msg->destiny) == 0) && (strcmp(aux->source, msg->source) == 0) && (aux->monitor == 1)) { //Mensagem recebida pelo destino
                                break;
                            }
                        }
                    }

                }
            }
            else{ // msg nao e token
                printf("Mensagem não é token\n");
                if (fila->tam !=0) { // Se tiver msg para enviar, compara o campo de hi_priority e substitui se for maior
                    if (msg->priority < fila->hi_priority) {
                        msg->priority = fila->hi_priority;
                    }
                }
                if (strcmp(msg->destiny, host) == 0) { // Checa remetente. Se for imprime, e seta o bit monitor
                    printf("Mensagem para mim\n");
                    msg->monitor = 1;
                    printf("Msg de %s:\n>%s\n",msg->source, msg->data);
                }
                c = (char *)msg;
                msg->crc = crc8(0,c,279);
                // Passa msg para frente
                printf("PAssando para frente source: %s\ndestiny: %s\n",msg->source, msg->destiny);
                sendto(sockdescr, msg, 280, 0, (struct sockaddr *) &sa, sizeof(sa));
            }
        }
	}
    return NULL;
}

main(int argc, char *argv[])
{
    int numbytesrecv;
    struct hostent *hp, *hps;
    char *dados, *c;

    unsigned int i;

    if((hp = gethostbyname(destiny)) == NULL){
      puts("Nao consegui obter endereco IP do servidor.");
      exit(1);
    }

    bcopy((char *)hp->h_addr, (char *)&sa.sin_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;

    sa.sin_port = htons(47593);//porta

    if((sockdescr=socket(hp->h_addrtype, SOCK_DGRAM, 0)) < 0) {
      puts("Nao consegui abrir o socket.");
      exit(1);
    }
    //Socket para receber
    if ((hps = gethostbyname(host)) == NULL){
                puts ("Nao consegui meu proprio IP");
                exit (1);
        }

        isa.sin_port = htons(47593);

        bcopy ((char *) hps->h_addr, (char *) &isa.sin_addr, hps->h_length);

        isa.sin_family = hps->h_addrtype;


        if ((socketserver = socket(hps->h_addrtype,SOCK_DGRAM,0)) < 0){
           puts ( "Nao consegui abrir o socket" );
                exit ( 1 );
        }

        if (bind(socketserver, (struct sockaddr *) &isa,sizeof(isa)) < 0){
                puts ( "Nao consegui fazer o bind" );
                exit ( 1 );
        }


    char buffer[275];
    fila = malloc(sizeof(Fila));
    init_fila(fila);
    Packet *token = malloc(sizeof(Packet));
    Packet *aux = malloc(sizeof(Packet));
    if(argv[1] != NULL){
        token->begin = 97;
        token->priority = 0;
        token->token = 1;
        token->monitor = 0;
        token->hi_priority = 0;
        copia_string(host, token->source);
        copia_string(destiny, token->destiny);
        token->size = 0;
        memset(token->data,0,DATA_MAXSIZE*sizeof(char));
        token->data[0] = '\n';
        c = (char *)token;
        printf("aqui\n");
        token->crc = crc8(0, c, 279);
		printf("Chora\n");
        sendto(sockdescr, token, 280, 0, (struct sockaddr *) &sa, sizeof(sa));
        struct timeval read_timeout;
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(socketserver, &readset);
        read_timeout.tv_sec = 5;
        read_timeout.tv_usec = 5000;
        int a = 100;
        a = select(socketserver+1,&readset,NULL,NULL,&read_timeout);
        printf("Envio do primeiro token%d\n", a);
        if (a == 1) { //Mensagem foi recebida
            recvfrom(socketserver, aux, 280, 0, (struct sockaddr *) &isa, sizeof(isa));
            printf("Lendo a confirmação: %s destiny: %s srtcmp: %d %d monitor:%d\n",aux->source, aux->destiny, strcmp(aux->destiny, aux->destiny), strcmp(aux->source, token->source), aux->monitor);
            if ((strcmp(aux->destiny, token->destiny) == 0) && (strcmp(aux->source, token->source) == 0) && (aux->monitor == 1)) { //Mensagem recebida pelo destino
                printf("confirmação de recebimento\n");
            }
        }
    }
// cria thread para escutar a rede
    pthread_t tid;
    int err = pthread_create(&tid, NULL, &escuta, NULL);
    if (err != 0){
            printf("\nCan't create thread :[%s]", strerror(err));
            exit(1);
    }
    while (1) {
        Packet *msg = malloc(sizeof(Packet));
        printf("Escreva e msg <destino><prioridade><msg>\n");
        fgets(buffer, 275, stdin);
        prepara_msg(msg, buffer, host);
        push_fila(fila, msg);
        printf("%d\n",fila->tam);
    }

    close(sockdescr);
    exit(0);
}

void prepara_msg(Packet *msg,char *buffer,char *host){
    int i,j;
    while (buffer[i] != ' ') {
        msg->destiny[i] = buffer[i];
        i++;
    }
    for (j = i; j < ADR_SIZE; j++) {
        msg->destiny[i] = 0;
    }
    i++;
    char c[2];
    c[0] = buffer[i];
    c[1] = '\n';
    msg->priority = atoi(c);

    i += 2;
    int k = 0;
    while (buffer[i] != '\n') {
        msg->data[k] = buffer[i];
        i++;
        k++;
    }
    strcpy(host, msg->source);
    msg->monitor = 0;
}

void init_fila (Fila *fila){
    fila->inicio = NULL;
    fila->fim = NULL;
    fila->hi_priority = 0;
    fila->tam = 0;
}

void push_fila(Fila *fila, Packet *msg){
    No *no = malloc(sizeof(No));
    no->msg = msg;
    no->prox = NULL;
    if (fila->tam == 0) {
        fila->inicio = no;
        fila->fim = no;
        fila->tam++;
        fila->hi_priority = msg->priority;
    }
    else{
        fila->fim->prox = no;
        fila->fim = no;
        fila->tam++;
        if (fila->hi_priority < msg->priority) {
            fila->hi_priority = msg->priority;
        }
    }
    return;
}

Packet *pop_fila(Fila *fila){
    No *no = fila->inicio;
    Packet *msg;
    if (no->msg->priority == fila->hi_priority){
        msg = no->msg;
        fila->inicio = no->prox;
        fila->tam--;
        free(no);
    }
    else {
        No *aux;
        while (no->prox != NULL) {
            aux = no->prox;
            if(aux->msg->priority == fila->hi_priority){
                msg = aux->msg;
                free(aux);
                fila->tam--;
                fila->hi_priority = 0;
                if (fila->tam == 0) {
                    fila->inicio = fila->fim = NULL;
                    break;
                }
                else{
                    no = fila->inicio;
                    while(no != NULL){
                        if (no->msg->priority > fila->hi_priority) {
                            fila->hi_priority = no->msg->priority;
                        }
                        no = no->prox;
                    }
                    break;
                }
            }
            no = no->prox;
        }
    }
    return msg;
}

void copia_string (char *entrada, char *copia)
{
    int i;
    for (i = 0; i < 10; i++){
	copia[i] = entrada[i];
    }
}

unsigned crc8(unsigned crc, char *data, size_t len)
{
    unsigned end;

    if (len == 0)
        return crc;
    crc ^= 0xff;
    // end = data + len;
    end = 0;
    do {
        crc = crc8_table[crc ^ data[end]];
        end++;
    } while (end < len);
    return crc ^ 0xff;
}

double timestamp(void){
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return((double)(tp.tv_sec*1000.0 + tp.tv_usec/1000.0));
}
