/*
 * Damien Dejean 2011
 *
 * Primitives du protocole de communication avec le serveur de son sur TX.
 */

#include <assert.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "protocol.h"
#include "messages.h"

/* Buffers d'émission et réception */
char *send_buf;
char *recv_buf;

/* 
 * Attend un message sur <socket> et le met dans le buffer pointé par <*buffer>,
 * attendu de taille <size>.
 * Si le buffer n'est pas alloué, il est alloué de taille <size> et retourné
 * dans <*buffer>.
 * Retourne 0 si réussi, -1 la réception a échouée.
 */
int spam_recv(int socket, char **buffer)
{
        int i, ret, msg_size;
        char *local;

        memset(recv_buf, 0, MSG_SIZE);

        /* Recevoir le message */
        for (i = 0, ret = -1; i < 100 && ret < 0; i++) {
                ret = recv(socket, recv_buf, 1022, 0);
                usleep(10000);
        }
        if (ret < 0) {
                error("Time out de réception dépassé.\n");
                return -1;
        }

        /* Recopier la partie utile du message */
        msg_size = strlen(recv_buf);
        msg_size = (msg_size > 1024) ? MSG_SIZE : msg_size + 1;
        local = (char*) calloc(msg_size, sizeof(char));
        strncpy(local, recv_buf, msg_size);
        *buffer = local;

        return 0;
}

/*
 * Attendre un acquitement ou une erreur de la part du serveur.
 * <number> le numéro de paquet
 * <error> un pointeur de pointeur sur une chaine de caractère, *error = NULL si
 * il n'y a pas eu d'erreur, sinon il pointe sur une chaine contenant une
 * description de l'erreur.
 * Retourne 0 si la reception s'est bien passée, -1 sinon.
 */
int spam_ack(int socket, int *number, char **error)
{
        char *ack, *emsg;
        char *cur;
        int pnumber;
        int ret, err;
        int i;

        assert(number != NULL);

        /* Recevoir un message */
        ret = spam_recv(socket, &ack);
        debug("Paquet recu: %s\n", ack);
        if (ret < -1) {
                error("Aucun message n'a été reçu pour l'acquitement.");
                err = -1;
                goto error;
        } 

        /* Detecter le numéro de paquet et le type de message */

        if (!strncmp(ack, "OK ", 3)) {
                sscanf(ack, "OK %d", &pnumber);
                debug("Le message recu est OK.\n");
                *number = pnumber;
                *error = NULL;
                free(ack);
                return 0;
        } 

        emsg = (char*) calloc(MSG_SIZE, sizeof(char));
        if (!strncmp(ack, "ERROR ", 6)) {
                sscanf(ack, "ERROR %d : %s", &pnumber, emsg);           // Récupérer le numéro de paquet
                for (i = 0, cur = ack; *cur != ':'; i++, cur++);        // Récupérer la chaine d'erreur en entrier
                strcpy(emsg, ack + i + 2);
        } else {
                error("Aucun message recu n'était valide, abandon.");
                err = -1;
                goto parse_error;
        }
        *number = pnumber;
        *error = emsg; 

        free(ack);
        return 0;

parse_error:
        free(emsg);
error:
        free(ack);
        return err;
}


/*
 * Initier la connexion avec le serveur sur le terminal.
 * Fournir les paramètres de connexion <connexion>,
 * retourne la version du serveur, la taille du buffer en cours, 
 * le port de données et la clef du serveur.
 * L'entier de retour indique l'état de l'ouverture.
 */
int spam_knock(conn_t *connection, int *buf_size, int *data_port, int *key) 
{
        int ret;
        int error;
        const char *send_msg = "SPAM";
        char *answer = NULL;
        float version;

        /* Envoyer le message de connexion */
        memset(send_buf, 0, MSG_SIZE);
        strncpy(send_buf, send_msg, strlen(send_msg));
        ret = send(connection->cmd_sock, send_buf, MSG_SIZE, 0);
        if (ret < 0) {
                error("Impossible d'envoyer le message de connexion.\n");
                error = -1;
                goto error;
        }

        /* Attendre le message de bienvenue */
        ret = spam_recv(connection->cmd_sock, &answer);
        if (ret < 0) {
                error("Message de bienvenue non recu !\n");
                error = -1;
                goto error_free;
        }
        
        /* Interpréter les valeurs */
        debug("Message reçu: %s\n", answer);
        sscanf(answer, 
               "SP4M v%f\nbuffer_size=%d\ndata_port=%d\ndata_key=%d", 
               &version,
               buf_size,
               data_port,
               key);
        if (version <= 0.0 && *buf_size <= 0 && *data_port <= 0) {
                error("Trame reçue invalide, abandon de la connexion.\n");
                error = -1;
                goto error_free;
        }

        free(answer);
        return 0;

error_free:
        free(answer);
error:
        *buf_size          = 0;
        *data_port         = 0;
        *key               = 0;
        return error;
}

/*
 * Envoie la clef <key> au serveur de donnée pour faire l'association entre la
 * connexion de commandes et la connexion de données.
 */
int spam_data_auth(conn_t *connection, int key) {
      
        int ret, error;
        char *msg;

        /* Envoyer le message de connexion */
        memset(send_buf, 0, MSG_SIZE);
        snprintf(send_buf, 16, "SPAM %d", key);
        ret = send(connection->data_sock, send_buf, MSG_SIZE, 0);
        if (ret < 0) {
                error("Impossible d'envoyer la clef au serveur de données.\n");
                error = -1;
                goto error;
        }

        /* Récupérer et vérifier la réponse */
        ret = spam_recv(connection->data_sock, &msg);
        if (strncmp(msg, "SP4M", 4)) {
                error("Réponse à la clef invalide.\n");
                error = -1;
                goto error;
        }

        free(msg);
        return 0;

error:
        free(msg);
        return error;
}


/*
 * Envoie un RESET sur la socket de commande pour le serveur de son.
 */
int spam_send_reset(conn_t *connection)
{
        int ret;
        char *msg = "RESET";

        /* Envoyer le message de reset */
        memset(send_buf, 0, MSG_SIZE);
        strncpy(send_buf, msg, 6); 
        ret = send(connection->cmd_sock, send_buf, MSG_SIZE, 0);
        if (ret < -1) {
                error("Impossible d'envoyer le signal de reset.");
                return -1;
        }

        return 0;
}

