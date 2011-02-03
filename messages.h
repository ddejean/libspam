/*
 * Damien Dejean 2011
 *
 * Fourni des primitives d'affichage pour les erreurs et le debug.
 */

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <stdio.h>

#ifdef _DEBUG_

#define debug(_msg,args...)             \
        do {                            \
                printf(_msg, ##args);   \
                fflush(stdout);         \
        } while (0)

#define error(_msg,args...)                                                             \
        do {                                                                            \
                fprintf(stderr, "Fichier: %s, ligne: %d - ", __FILE__, __LINE__);       \
                fprintf(stderr, _msg, ##args);                                          \
                fflush(stderr);                                                         \
        } while (0)


#else

#define debug(_msg,args...)             do {} while (0)

#define error(_msg,args...)                     \
        do {                                    \
                fprintf(stderr, _msg, ##args);  \
                fflush(stderr);                 \
        } while (0)


#endif

#endif

