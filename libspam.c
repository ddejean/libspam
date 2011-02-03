/*
 * Damien Dejean 2011
 *
 * Bibliothèqe de communication avec le serveur de son spamMyTx.
 */

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>

#include "messages.h"
#include "protocol.h"




conn_t *spam_connect(char *tx_name) {

        int ret;
        int cmd_sock, data_sock;
        int buf_size, data_port, key;
        struct hostent *tx_host;
        struct sockaddr_in tx_addr;
        conn_t *connexion;


        /* Récupérer l'adresse du TX */
        assert(tx_name != NULL);
        tx_host = gethostbyname(tx_name);
        if (!tx_host) {
                error("Impossible d'obtenir l'adresse de l'hôte %s.\n", tx_name);
                goto error;
        }
        tx_addr.sin_family      = AF_INET;
        tx_addr.sin_addr.s_addr = ((struct in_addr*)(tx_host->h_addr))->s_addr;
        tx_addr.sin_port        = htons(CMD_PORT);

        /* Ouvrir les deux sockets */
        cmd_sock = socket(PF_INET, SOCK_STREAM, 0);
        if (cmd_sock < 0) {
                error("Impossible d'ouvrir la socket \"cmd_sock\".\n");
                goto error;
        }
        data_sock = socket(PF_INET, SOCK_STREAM, 0);
        if (data_sock < 0) {
                error("Impossible d'ouvrir la socket \"data_sock\".\n");
                goto err_data_sock;
        }

        /* Allouer la structure des données de connexion */
        connexion = (conn_t*) calloc(1, sizeof(conn_t));
        connexion->cmd_sock = cmd_sock;
        connexion->data_sock = data_sock;


        /* Initialiser la connexion de commandes */
        ret = connect(cmd_sock, (struct sockaddr*)&tx_addr, sizeof(struct sockaddr_in));
        if (ret < 0) goto err_connect;
        spam_knock(connexion, &buf_size, &data_port, &key);
        debug("Connexion réussie: buf_size = %d, data_port = %d, key = %d\n",
                        buf_size,
                        data_port,
                        key);

        /* Initialiser la connexion de données et authentifier la connexion */
        tx_addr.sin_port = htons(data_port);
        ret = connect(data_sock, (struct sockaddr*)&tx_addr, sizeof(struct sockaddr_in));
        if (ret < 0) goto err_connect;
        spam_data_auth(connexion, key);

        return connexion;

err_connect:
        close(data_sock);
        free(connexion);
err_data_sock:
        close(cmd_sock);
error:
        return NULL;
}

/*
 * Termine la connexion avec le serveur de son et ferme proprement les sockets
 * ouvertes.
 */
int spam_disconnect(conn_t *connexion) 
{
        int ret, error;
        char *send_msg, *recv_msg;

        send_msg = (char*) calloc(128, sizeof(char));
        snprintf(send_msg, 9, "%s", "END SPAM"); 

        /* Envoyer le message de fermeture */
        ret = send(connexion->cmd_sock, send_msg, 9, 0);
        if (ret < 0) {
                error("Impossible de fermer la connexion au serveur.\n");
                error = -1;
                goto error_send;
        }

        /* Récupérer et vérifier la réponse */
        recv_msg = (char*) calloc(16, sizeof(char));
        ret = spam_recv(connexion->cmd_sock, &recv_msg, 8);
        if (ret < 0) {
                error("Message de fin de connexion non acquitté.\n");
                error = -1;
                goto error_recv;
        }
        /* Pour le moment recevoir un paquet indique que c'est bon
         * il faudra revoir ça pour recevoir le bon ACK.
        if (strncmp(recv_msg, "END SP4M", 8)) {
                error("Réponse à la fermeture invalide.\n");
                error = -1;
                goto error_recv;
        }
        */

        /* Libérer les sockets ouvertes */
        close(connexion->cmd_sock);
        close(connexion->data_sock);
        memset(connexion, 0, sizeof(conn_t));

        free(recv_msg);
        free(send_msg);
        return 0;

error_recv:
        free(recv_msg);
error_send:
        free(send_msg);
        return error;
}

int main(int argc, char **argv)
{
        conn_t *connexion;

        if (argc != 2) {
                error("Mauvais nombre d'arguments !\n");
                return -1;
        }

        connexion = spam_connect(argv[1]);
        usleep(1000000);
        spam_disconnect(connexion);
        free(connexion);

        return 0;
}
