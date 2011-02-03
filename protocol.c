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

/* 
 * Attend un message sur <socket> et le met dans le buffer pointé par <*buffer>,
 * attendu de taille <size>.
 * Si le buffer n'est pas alloué, il est alloué de taille <size> et retourné
 * dans <*buffer>.
 * Retourne 0 si réussi, -1 la réception a échouée.
 */
int spam_recv(int socket, char **buffer, int size)
{
        int i, ret;
        char *local;

        assert(size > 0);

        if (!(*buffer))
                local = (char*) calloc(size, sizeof(char));
        else 
                local = *buffer;

        for (i = 0, ret = -1; i < 500 && ret < 0; i++) {
                ret = recv(socket, local, size, 0);
                usleep(10000);
        }
        if (ret < 0) {
                error("Time out de réception dépassé.\n");
                if (!(*buffer)) {
                        free(local);
                        *buffer = NULL;
                }
                return -1;
        }
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
int spam_ack(conn_t *connexion, int *number, char **error)
{
        char *ack, *emsg;
        int pnumber;
        int ret, error;

        assert(number != NULL);
        assert(connexion != NULL);

        /* Recevoir un message */
        ack = (char*) calloc(256, sizeof(char));
        ret = spam_recv(connexion->cmd_sock, &ack, 256);
        if (ret < -1) {
                error("Aucun message n'a été reçu pour l'acquitement.");
                error = -1
                goto error;
        } 

        /* Detecter le numéro de paquet et le type de message */
        sscanf(ack, "OK %d", &pnumber);
        if (pnumber > 0) {
                debug("Le message recu est OK.");
                *number = pnumber;
                *error = NULL;
                free(ack);
                return 0;
        } 

        emsg = (char*) calloc(256, sizeof(char));
        sscanf(ack, "ERROR %d : %s", &pnumber, emsg);
        if (pnumber <= 0) {
                error("Aucun message recu n'était valide, abandon.");
                error = -1;
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
        return error;
}


/*
 * Initier la connexion avec le serveur sur le terminal.
 * Fournir les paramètres de connexion <connexion>,
 * retourne la version du serveur, la taille du buffer en cours, 
 * le port de données et la clef du serveur.
 * L'entier de retour indique l'état de l'ouverture.
 */
int spam_knock(conn_t *connexion, int *buf_size, int *data_port, int *key) 
{
        int ret;
        int error;
        const char *send_msg = "SPAM";
        char *answer = NULL;
        float version;

        /* Envoyer le message de connexion */
        ret = send(connexion->cmd_sock, send_msg, 4, 0);
        if (ret < 0) {
                error("Impossible d'envoyer le message de connexion.\n");
                error = -1;
                goto error;
        }

        /* Attendre le message de bienvenue */
        ret = spam_recv(connexion->cmd_sock, &answer, 128);
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
int spam_data_auth(conn_t *connexion, int key) {
      
        int ret, error;
        char *msg;

        /* Préparer le message */
        msg = (char*) calloc(16, sizeof(char));
        snprintf(msg, 16, "SPAM %d", key);

        /* Envoyer le message de connexion */
        ret = send(connexion->data_sock, msg, 16, 0);
        if (ret < 0) {
                error("Impossible d'envoyer la clef au serveur de données.\n");
                error = -1;
                goto error;
        }

        /* Récupérer et vérifier la réponse */
        memset(msg, 0, 16);
        ret = spam_recv(connexion->data_sock, &msg, 16);
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
int spam_send_reset(conn_t *connexion)
{
        int ret;
        char *msg = "RESET";

        /* Envoyer le message de reset */
        ret = send(connexion->cmd_sock, msg, 5, 0);
        if (ret < -1) {
                error("Impossible d'envoyer le signal de reset.");
                return -1
        }

        return 0;
}

