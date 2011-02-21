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

/* Etablit une connexion avec le serveur de son distant */
conn_t *spam_connect(char *tx_name) {

        int ret;
        int cmd_sock, data_sock;
        int buf_size, data_port, key;
        struct hostent *tx_host;
        struct sockaddr_in tx_cmd;
        struct sockaddr_in tx_data;
        conn_t *connection;


        /* Récupérer l'adresse du TX */
        assert(tx_name != NULL);
        tx_host = gethostbyname(tx_name);
        if (!tx_host) {
                error("Impossible d'obtenir l'adresse de l'hôte %s.\n", tx_name);
                goto error;
        }
        tx_cmd.sin_family      = AF_INET;
        tx_cmd.sin_addr.s_addr = ((struct in_addr*)(tx_host->h_addr))->s_addr;
        tx_cmd.sin_port        = htons(CMD_PORT);

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
        connection               = (conn_t*) calloc(1, sizeof(conn_t));
        connection->cmd_sock     = cmd_sock;
        connection->data_sock    = data_sock;

        /* Préparer les buffers */
        send_buf = (char*) calloc(BUF_SIZE, sizeof(char));
        recv_buf = (char*) calloc(BUF_SIZE, sizeof(char));

        /* Initialiser la connexion de commandes */
        ret = connect(cmd_sock, (struct sockaddr*)&tx_cmd, sizeof(struct sockaddr_in));
        if (ret < 0) {
                error("Impossible d'ouvrir la connexion sur \"cmd_sock\".\n");
                goto err_connect;
        }
        spam_knock(connection, &buf_size, &data_port, &key);
        debug("Connexion réussie: buf_size = %d, data_port = %d, key = %d\n",
                        buf_size,
                        data_port,
                        key);

        /* Initialiser la connexion de données et authentifier la connexion */
        tx_data.sin_family      = AF_INET;
        tx_data.sin_addr.s_addr = ((struct in_addr*)(tx_host->h_addr))->s_addr;
        tx_data.sin_port        = htons(data_port);
        ret = connect(data_sock, (struct sockaddr*)&tx_data, sizeof(struct sockaddr_in));
        if (ret < 0) {
                error("Impossible d'ouvrir la connexion sur \"data_sock\".\n");
                goto err_connect;
        }
        spam_data_auth(connection, key);

        return connection;

err_connect:
        close(data_sock);
        free(connection);
err_data_sock:
        close(cmd_sock);
error:
        return NULL;
}

/*
 * Termine la connexion avec le serveur de son et ferme proprement les sockets
 * ouvertes.
 */
int spam_disconnect(conn_t *connection) 
{
        int ret, error;
        char *recv_msg;


        /* Envoyer le message de fermeture */
        memset(send_buf, 0, MSG_SIZE);
        snprintf(send_buf, 9, "%s", "END SPAM"); 
        ret = send(connection->cmd_sock, send_buf, 1022, 0);
        if (ret < 0) {
                error("Impossible de fermer la connexion au serveur.\n");
                error = -1;
                goto error;
        }

        /* Récupérer et vérifier la réponse */
        ret = spam_recv(connection->cmd_sock, &recv_msg);
        if (ret < 0) {
                error("Message de fin de connexion non acquitté.\n");
                error = -1;
                goto error;
        }
        if (strncmp(recv_msg, "END SP4M", 8)) {
                error("Réponse à la fermeture invalide.\n");
                error = -1;
                free(recv_msg);
                goto error;
        }

        /* Libérer les sockets ouvertes */
        close(connection->cmd_sock);
        close(connection->data_sock);
        memset(connection, 0, sizeof(conn_t));

        /* Libérer les buffers */
        free(recv_buf);
        free(send_buf);
        free(recv_msg);
        return 0;

error:
        free(recv_buf);
        free(send_buf);
        return error;
}

/* 
 * Envoi un signal de RESET au serveur au bout de la connexion <connexion>.
 */
int spam_reset(conn_t *connection) 
{
        int ret;
        int packet;
        char *errorc;

        ret = spam_send_reset(connection);
        if (ret < 0) 
                return -1;

        ret = spam_ack(connection->cmd_sock, &packet, &errorc);
        if (ret < 0) 
                return -1;
        else {
                debug("Reset %d acquitté.", packet);
                if (errorc != NULL) {
                        debug(" Erreur: %s\n", errorc); 
                        free(errorc);
                        return -1;
                } else {
                        debug("\n");
                }
        }
        return 0;
}

/*
 * Configurer le DSP de la machine distante.
 */
int spam_config_dsp(conn_t *connection, int size, int channel, int rate)
{
        int ret;
        int packet;
        char *error;

        /* Construire et envoyer la commande */
        memset(send_buf, 0, BUF_SIZE);
        sprintf(send_buf, "CONF_DSP\nsize=%d\nchannel=%d\nrate=%d", size, channel, rate);
        ret = send(connection->cmd_sock, send_buf, MSG_SIZE, 0);
        if (ret < 0) {
                error("Impossible d'envoyer la paquet de configuration.\n");
                return -1;
        }

        /* Attendre et valider l'acquittement */
        ret = spam_ack(connection->cmd_sock, &packet, &error);
        if (ret < 0) {
                error("Impossible de recevoir l'acquittement.\n");
                return -1;
        }
        if (error == NULL) {
                return 0;
        } else {
                error("Erreur pour la commande CONF DSP: %s\n", error);
                free(error);
                return -1;
        }
}


/* 
 * Configuration du volume de la machine distante 
 */
int spam_volume(conn_t *connection, int left, int right)
{
        int ret;
        int packet;
        char *error;

        /* Construire et envoyer la commande */
        memset(send_buf, 0, BUF_SIZE);
        sprintf(send_buf, "VOLUME\nleft=%d\nright=%d", left, right);
        ret = send(connection->cmd_sock, send_buf, MSG_SIZE, 0);
        if (ret < 0) {
                error("Impossible d'envoyer la paquet de changement de volume.\n");
                return -1;
        }

        /* Attendre et valider l'acquittement */
        ret = spam_ack(connection->cmd_sock, &packet, &error);
        if (ret < 0) {
                error("Impossible de recevoir l'acquittement.\n");
                return -1;
        }
        if (error == NULL) {
                return 0;
        } else {
                error("Erreur pour la commande VOLUME: %s\n", error);
                free(error);
                return -1;
        }
}

/* 
 * Configuration de la taille de buffer du serveur.
 */
int spam_buffer(conn_t *connection, int buf_size)
{
        int ret;
        int packet;
        char *error;

        /* Construire et envoyer la commande */
        memset(send_buf, 0, BUF_SIZE);
        sprintf(send_buf, "BUFFER %d", buf_size);
        ret = send(connection->cmd_sock, send_buf, MSG_SIZE, 0);
        if (ret < 0) {
                error("Impossible d'envoyer la commande de changement de buffer.\n");
                return -1;
        }

        /* Attendre et valider l'acquittement */
        ret = spam_ack(connection->cmd_sock, &packet, &error);
        if (ret < 0) {
                error("Impossible de recevoir l'acquittement.\n");
                return -1;
        }
        if (error == NULL) {
                connection->buf_size = buf_size;
                return 0;
        } else {
                error("Erreur pour la commande BUFFER: %s\n", error);
                free(error);
                return -1;
        }
}

/*
 * Envoyer des données au serveur de son. Attend un paquet buffer de la taille
 * du buffer de données spécifié au serveur. 
 */
int spam_data(conn_t *connection, char *buffer)
{
        int ret;
        int packet;
        char *error;

        /* Copier les données dans le buffer d'envoi et zou ! */
        ret = send(connection->data_sock, buffer, connection->buf_size, 0);
        if (ret < 0) {
                error("Impossible d'envoyer le paquet de données.\n");
                return -1;
        }

        /* Attendre et valider l'acquittement */
        ret = spam_ack(connection->data_sock, &packet, &error);
        if (ret < 0) {
                error("Impossible de recevoir l'acquittement.\n");
                return -1;
        }
        if (error == NULL) {
                return 0;
        } else {
                error("Erreur d'envoi de données : %s\n", error);
                free(error);
                return -1;
        }
}


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int main(int argc, char **argv)
{
        int fd;
        int size, channel, rate;
        char *buffer;
        conn_t *connection;

        if (argc != 3) {
                error("Mauvais nombre d'arguments !\n");
                return -1;
        }

        connection = spam_connect(argv[1]);
        if (connection == NULL) {
                error("Abandon\n");
                return -1;
        }
        usleep(1000000);
        spam_reset(connection);
        spam_config_dsp(connection, 16, 2, 44100);
        spam_volume(connection, 80, 80);
        spam_buffer(connection, 60000);

        fd = open(argv[2], O_RDONLY);
        buffer = (char*) calloc(46, sizeof(char));
        read(fd, buffer, 46);

	channel = buffer[22]+(buffer[23]<<8);
	rate = buffer[24]+(buffer[25]<<8)+(buffer[26]<<16)+(buffer[27]<<24);
	size = buffer[34];

        spam_config_dsp(connection, 16, 2, 44100);
        free(buffer);

        buffer = (char*) calloc(60000, sizeof(char));
        while (read(fd, buffer, 60000))
                spam_data(connection, buffer);


        spam_disconnect(connection);
        free(connection);

        return 0;
}

