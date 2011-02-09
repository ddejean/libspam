/*
 * Damien Dejean 2011
 *
 * Primitives du protocole de communication avec le serveur de son sur TX.
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define         CMD_PORT        0x1337
#define         MSG_SIZE        1022            /* Taille d'un message */
#define         DATA_SIZE       1024            /* Taille d'un paquet de données */
#define         BUF_SIZE        1024            /* Taille d'un buffer */

/* Buffers d'émission et réception */
extern char *send_buf;
extern char *recv_buf;

/* Structure décrivant une connexion */
typedef struct _conn_t {
        int cmd_sock;           /* Socket de commandes */
        int data_sock;          /* Socket de données */
        int buf_size;           /* Taille du buffer distant */
} conn_t;

/* 
 * Attend un message sur <socket> et le met dans le buffer pointé par <*buffer>,
 * attendu de taille <size>.
 * Si le buffer n'est pas alloué, il est alloué de taille <size> et retourné
 * dans <*buffer>.
 * Retourne 0 si réussi, -1 la réception a échouée.
 */
int spam_recv(int socket, char **buffer);

/*
 * Attendre un acquitement ou une erreur de la part du serveur.
 * <number> le numéro de paquet
 * <error> un pointeur de pointeur sur une chaine de caractère, *error = NULL si
 * il n'y a pas eu d'erreur, sinon il pointe sur une chaine contenant une
 * description de l'erreur.
 * Retourne 0 si la reception s'est bien passée, -1 sinon.
 */
int spam_ack(int socket, int *number, char **error);


/*
 * Initier la connexion avec le serveur sur le terminal.
 * Fournir les paramètres de connexion <connexion>,
 * retourne la version du serveur, la taille du buffer en cours, 
 * le port de données et la clef du serveur.
 * L'entier de retour indique l'état de l'ouverture.
 */
int spam_knock(conn_t *connexion, int *buf_size, int *data_port, int *key);

/*
 * Envoie la clef <key> au serveur de donnée pour faire l'association entre la
 * connexion de commandes et la connexion de données.
 */
int spam_data_auth(conn_t *connexion, int key);

/*
 * Envoie un RESET sur la socket de commande pour le serveur de son.
 */
int spam_send_reset(conn_t *connexion);

#endif 

