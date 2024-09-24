#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TABELLA_SIZE 10
typedef struct Ingrediente {
    char nome_ingrediente[255];
    int quantita;
    struct Ingrediente *next;
} Ingrediente;

typedef struct Ricetta {
    char nome_ricetta[255];
    Ingrediente *ingredienti; // lista concatenata di ingredienti
    struct Ricetta *next; // Per la lista concatenata di ricette
    int peso; //somma dei grammi dei singoli ingredienti
} Ricetta;

typedef struct {
    Ricetta *ricette[TABELLA_SIZE];
} Catalogo;

typedef struct Lotto {
    char *nome_ingrediente;
    int quantita;
    int data_scadenza;
    struct Lotto *next;
} Lotto;

typedef struct Ordine {
    int istante_arrivo;
    int quantita; //quantità di quella ricetta
    Ricetta ricetta;
    struct Ordine *next;
    int peso; // somma degli ingredienti della ricetta * quantità
} Ordine;

typedef struct {
    Lotto *lotti[TABELLA_SIZE];
    Ordine *ordini_attesa; //lista concatenata di ordini in attesa
    Ordine *ordini_pronti; //lista concatenata di ordini pronti
} Magazzino;


typedef struct {
    int capienza_camion;
    int periodicita;
    Ordine *ordini;
} Corriere;

//VARIIABILI GLOBALI
Catalogo catalogo;
Corriere corriere;
Magazzino magazzino;


//FUNZIONE DI HASH
unsigned int murmurhash(const char *key, int len) {
    const unsigned int seed = 0x5bd1e995;
    unsigned int hash = seed ^ len;
    const unsigned char *data = (const unsigned char *) key;

    while (len >= 4) {
        unsigned int k = *(unsigned int *) data;
        k *= seed;
        k ^= k >> 24;
        k *= seed;

        hash *= seed;
        hash ^= k;

        data += 4;
        len -= 4;
    }

    // Trattamento per i byte rimanenti
    switch (len) {
        case 3: hash ^= data[2] << 16;
        case 2: hash ^= data[1] << 8;
        case 1: hash ^= data[0];
            hash *= seed;
        default: ;
    };

    // Mescola ulteriormente il risultato
    hash ^= hash >> 13;
    hash *= seed;
    hash ^= hash >> 15;

    return hash % TABELLA_SIZE;
}

unsigned int hash_ricetta(const char *nome_ricetta) {
    return murmurhash(nome_ricetta, strlen(nome_ricetta));
}

unsigned int hash_ingrediente(const char *nome_ingrediente) {
    return murmurhash(nome_ingrediente, strlen(nome_ingrediente));
}

//METODI

//per creare la ricetta gli metto la lista concatenata degli ingredienti
void aggiungiIngredienteAllaRicetta(Ricetta *ricetta, char *nome_ingrediente, int quantita) {
    //il lotto viene scelto in base alle esigenze della ricetta

    Ingrediente *nuovo_ingrediente = malloc(sizeof(Ingrediente));
    if (nuovo_ingrediente == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria!\n");
        exit(EXIT_FAILURE);
    }
    //CREA NUOVO INGREDIENTE
    //copio il nome dell'ingrediente
    strncpy(nuovo_ingrediente->nome_ingrediente, nome_ingrediente, sizeof(nuovo_ingrediente->nome_ingrediente));
    //aggiungo il carattere di terminazione della stringa
    nuovo_ingrediente->nome_ingrediente[sizeof(nuovo_ingrediente->nome_ingrediente) - 1] = '\0';
    //imposto la quantità
    nuovo_ingrediente->quantita = quantita;


    //aggiungo l'ingrediente alla lista concatenata
    nuovo_ingrediente->next = ricetta->ingredienti;
    ricetta->ingredienti = nuovo_ingrediente;
    ricetta->peso += quantita;
}

//creo il lotto e lo aggiungo al magazzino in ordine di data di scadenza
void aggiungiLottoAlMagazzino(char *nome_ingrediente, int quantita, int data_scadenza) {
    int hash = hash_ingrediente(nome_ingrediente);
    Lotto *nuovo_lotto = malloc(sizeof(Lotto));
    if (nuovo_lotto == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria!\n");
        exit(EXIT_FAILURE);
    }
    //CREA NUOVO LOTTO
    //copio il nome dell'ingrediente
    // Allocate memory for nome_ingrediente
    nuovo_lotto->nome_ingrediente = malloc(255 * sizeof(char));
    if (nuovo_lotto->nome_ingrediente == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria!\n");
        exit(EXIT_FAILURE);
    }

    // Copy the string and ensure null-termination
    strncpy(nuovo_lotto->nome_ingrediente, nome_ingrediente, 254);
    nuovo_lotto->nome_ingrediente[254] = '\0';
    nuovo_lotto->quantita = quantita;
    //imposto la data di scadenza
    nuovo_lotto->data_scadenza = data_scadenza;
    nuovo_lotto->next = NULL;

    //aggiungo il lotto all'array di lotti in base alla data di scadenza
    Lotto *current = magazzino.lotti[hash];
    Lotto *prec = NULL;
    while (current != NULL) {
        if (current->data_scadenza > data_scadenza) {
            if (prec == NULL) {
                nuovo_lotto->next = current;
                magazzino.lotti[hash] = nuovo_lotto;
            } else {
                prec->next = nuovo_lotto;
                nuovo_lotto->next = current;
            }
            break;
        }
        if (current->data_scadenza == data_scadenza && strcmp(current->nome_ingrediente, nome_ingrediente) == 0) {
            current->quantita += quantita;
            break;
        }
        prec = current;
        current = current->next;
    }
    if (current == NULL) {
        if (prec == NULL) {
            magazzino.lotti[hash] = nuovo_lotto;
        } else {
            prec->next = nuovo_lotto;
        }
    }
}

//aggiungo la ricetta al catalogo
void aggiungiRicettaAlCatalogo(Ricetta *nuova_ricetta) {
    int hash = hash_ricetta(nuova_ricetta->nome_ricetta);

    // Cerca se la ricetta è già presente nella lista concatenata all'indice hash
    Ricetta *ricetta = catalogo.ricette[hash];
    while (ricetta != NULL) {
        if (strcmp(nuova_ricetta->nome_ricetta, ricetta->nome_ricetta) == 0) {
            // Ricetta già presente, non aggiungere
            printf("ignorato\n");
            return;
        }
        ricetta = ricetta->next;
    }

    // Se non trovata, aggiungi la nuova ricetta
    nuova_ricetta->next = catalogo.ricette[hash];
    catalogo.ricette[hash] = nuova_ricetta;
    printf("aggiunta\n");
}

//creo un ordine
Ordine *creaOrdine(char *nome_ricetta, int quantita, int istante_arrivo) {
    //cerca la ricetta nel catalogo
    Ordine *ordine = malloc(sizeof(Ordine));
    int hash = hash_ricetta(nome_ricetta);
    Ricetta *ricetta = catalogo.ricette[hash];
    while (ricetta != NULL) {
        if (strcmp(nome_ricetta, ricetta->nome_ricetta) == 0) {
            ordine->ricetta = *ricetta;
            ordine->quantita = quantita;
            ordine->istante_arrivo = istante_arrivo;
            ordine->peso = quantita * ricetta->peso;
            ordine->next = NULL;
            fflush(stdout);
            printf("accettato\n");
            return ordine;
        }
        ricetta = ricetta->next;
    }
    printf("rifiutato\n");
    return NULL;
}

void inizializzaRicetta(Ricetta *r, char *nome_ricetta) {
    //copio il nome della ricetta
    strncpy(r->nome_ricetta, nome_ricetta, sizeof(r->nome_ricetta));
    //aggiungo il carattere di terminazione della stringa
    r->nome_ricetta[sizeof(r->nome_ricetta) - 1] = '\0';
    r->next = NULL;
    r->peso = 0;
    r->ingredienti=NULL;
}

void inizializzaCatalogo() {
    for (int i = 0; i < TABELLA_SIZE; i++) {
        catalogo.ricette[i] = NULL;
    }
}

void inizializzaMagazzino() {
    for (int i = 0; i < TABELLA_SIZE; ++i) {
        magazzino.lotti[i] = NULL;
    }
    magazzino.ordini_attesa = NULL;
    magazzino.ordini_pronti = NULL;
}

void inizializzaCorriere() {
    corriere.capienza_camion = 0;
    corriere.periodicita = 0;
    corriere.ordini = NULL;
}

//serve per eliminare gli ingredienti di una ricetta che devo eliminare
void liberaIngredientiDiUnaRicetta(Ricetta *r) {
    if (r == NULL)
        return;
    for (int i = 0; i < TABELLA_SIZE; i++) {
        //scorro tutta la tabella hash
        Ingrediente *ingrediente = r->ingredienti;
        while (ingrediente != NULL) {
            //scorro tutta la lista concatenata di ingredienti
            Ingrediente *temp = ingrediente; //salvo il puntatore all'ingrediente corrente
            ingrediente = ingrediente->next; //passo all'ingrediente successivo
            free(temp); //libero la memoria dell'ingrediente corrente
        }
    }
    r->ingredienti = NULL;
}
//cerca una ricetta
int cercaRicetta(char *nome) {
    Ricetta *current = catalogo.ricette[hash_ricetta(nome)];
    while (current != NULL) {
        if (strcmp(current->nome_ricetta, nome) == 0)
            return 1; //se è già presente
        current = current->next;
    }
    return 0; //se non è presente
}

//rimuove una ricetta dal catalogo
void rimuoviRicetta(char *nome_ricetta) {
    int hash = hash_ricetta(nome_ricetta);

    // Cerca se la ricetta è presente nella lista concatenata all'indice hash
    Ricetta *ricetta = catalogo.ricette[hash];
    Ricetta *ricetta_prec = NULL;
    while (ricetta != NULL) {
        if(cercaRicetta(nome_ricetta)==1) {
            printf("ordini in sospeso\n");
            return;
        }
        if (strcmp(nome_ricetta, ricetta->nome_ricetta) == 0) {
            // Ricetta trovata, rimuovi
            if (ricetta_prec == NULL) {
                // La ricetta è la prima della lista
                catalogo.ricette[hash] = ricetta->next;
            } else {
                // La ricetta è in mezzo alla lista
                ricetta_prec->next = ricetta->next;
            }
            liberaIngredientiDiUnaRicetta(ricetta);
            free(ricetta);
            printf("rimosso\n");
            return;
        }
        ricetta_prec = ricetta;
        ricetta = ricetta->next;
    }

    // Ricetta non trovata
    printf("non presente\n");
}

//serve per stampare il contenuto del camion oppure dire se è vuoto
void stampaCamion() {
    if (corriere.ordini == NULL)
    // ReSharper disable once CppDFAUnreachableCode
        printf("camioncino vuoto\n");
    else {
        Ordine *ordine = corriere.ordini;
        while (ordine != NULL) {
            printf("%d %s %d\n", ordine->istante_arrivo, ordine->ricetta.nome_ricetta, ordine->quantita);
            ordine = ordine->next;
        }
    }
}


//restituisce il lotto con la scadenza minore di uno specifico ingrediente
int trovaLottiNecessariPerIngrediente(Lotto **listalotti, char *nome_ingrediente, int quantita_necessaria) {
    int tot_trovato = 0;
    int hash = hash_ingrediente(nome_ingrediente);
    Lotto *lotto_magazzino = magazzino.lotti[hash];
    if (lotto_magazzino == NULL) {
        return 0;
    }
    while (lotto_magazzino != NULL && tot_trovato < quantita_necessaria) {
        if (strcmp(lotto_magazzino->nome_ingrediente, nome_ingrediente) == 0) {
            //1. se la quantità del lotto è maggiore o uguale alla quantità necessaria
            if (lotto_magazzino->quantita >= quantita_necessaria) {
                Lotto *nuovo_lotto = malloc(sizeof(Lotto));
                if (nuovo_lotto == NULL) {
                    fprintf(stderr, "Errore di allocazione della memoria!\n");
                    exit(EXIT_FAILURE);
                }
                nuovo_lotto->nome_ingrediente = lotto_magazzino->nome_ingrediente;
                nuovo_lotto->quantita = lotto_magazzino->quantita - quantita_necessaria + tot_trovato;
                nuovo_lotto->data_scadenza = lotto_magazzino->data_scadenza;
                if (*listalotti == NULL) {
                    nuovo_lotto->next = NULL;
                    *listalotti = nuovo_lotto;
                } else {
                    nuovo_lotto->next = *listalotti;
                    *listalotti = nuovo_lotto;
                }
                tot_trovato += quantita_necessaria - tot_trovato;
            } else {
                //2. se la quantità del lotto è minore della quantità necessaria
                Lotto *nuovo_lotto = malloc(sizeof(Lotto));
                if (nuovo_lotto == NULL) {
                    fprintf(stderr, "Errore di allocazione della memoria!\n");
                    exit(EXIT_FAILURE);
                }
                nuovo_lotto->nome_ingrediente = lotto_magazzino->nome_ingrediente;
                nuovo_lotto->quantita = 0;
                nuovo_lotto->data_scadenza = lotto_magazzino->data_scadenza;
                if (*listalotti == NULL) {
                    nuovo_lotto->next = NULL;
                    *listalotti = nuovo_lotto;
                } else {
                    nuovo_lotto->next = *listalotti;
                    *listalotti = nuovo_lotto;
                }
                tot_trovato += lotto_magazzino->quantita;
            }
        }
        lotto_magazzino = lotto_magazzino->next;
    }
    if (tot_trovato == quantita_necessaria) {
        return 1;
    }
    return 0;
}

//aggiungo in lista di attesa un ordine a seconda dell' istante di arrivo
void aggiungiOrdineInAttesa(Ordine *ordine) {
    Ordine *nuovo = malloc(sizeof(Ordine));
    memcpy(nuovo, ordine, sizeof(Ordine));
    nuovo->next = NULL;
    // Se la lista è vuota o se l'ordine deve essere inserito all'inizio
    if (magazzino.ordini_attesa == NULL) {
        magazzino.ordini_attesa = nuovo;
    } else {
        // Trova il punto giusto nella lista concatenata per inserire il nuovo ordine
        Ordine *current = magazzino.ordini_attesa;
        while (current->next != NULL && current->next->istante_arrivo <= nuovo->istante_arrivo) {
            current = current->next;
        }
        // Inserisci il nuovo ordine nella posizione corretta
        nuovo->next = current->next;
        current->next = nuovo;
    }
}

//aggiungo in lista di pronto un ordine a seconda dell' istante di arrivo
void aggiungiOrdineInPronto(Ordine *ordine) {
    Ordine *nuovo = malloc(sizeof(Ordine));
    memcpy(nuovo, ordine, sizeof(Ordine));
    nuovo->next = NULL;

    // Se la lista è vuota o se l'ordine deve essere inserito all'inizio
    if (magazzino.ordini_pronti == NULL) {
        magazzino.ordini_pronti = nuovo;
    } else {
        // Trova il punto giusto nella lista concatenata per inserire il nuovo ordine
        Ordine *current = magazzino.ordini_pronti;
        while (current->next != NULL && current->next->istante_arrivo <= nuovo->istante_arrivo) {
            current = current->next;
        }
        // Inserisci il nuovo ordine nella posizione corretta
        nuovo->next = current->next;
        current->next = nuovo;
    }
}

//aggiungo al corriere un ordine a seconda dell' istante di arrivo
void aggiungiOrdineAlCorriere(Ordine *ordine) {
    //fai una copia di ordine
    Ordine *nuovo = malloc(sizeof(Ordine));
    memcpy(nuovo, ordine, sizeof(Ordine));
    nuovo->next = NULL;
    // Se la lista è vuota o se l'ordine deve essere inserito all'inizio
    if (corriere.ordini == NULL || corriere.ordini->peso < nuovo->peso) {
        nuovo->next = corriere.ordini;
        corriere.ordini = nuovo;
    } else {
        // Trova il punto giusto nella lista concatenata per inserire il nuovo ordine
        Ordine *current = corriere.ordini;
        //devo ordinarli in maniera decrescente di peso
        while (current->next != NULL && current->next->peso >= nuovo->peso) {
            current = current->next;
        }
        // Inserisci il nuovo ordine nella posizione corretta
        nuovo->next = current->next;
        current->next = nuovo;
    }
}

//elimina/modifica i lotti che appartengono a un ordine che è pronto
void eliminaLotti(Lotto *listalotti) {
    Lotto *current = listalotti;
    while (current != NULL) {
        int hash = hash_ingrediente(current->nome_ingrediente);
        Lotto *lotto = magazzino.lotti[hash];
        Lotto *prec_lotto = NULL;
        while (lotto != NULL) {
            if (strcmp(lotto->nome_ingrediente, current->nome_ingrediente) == 0 && lotto->data_scadenza == current->
                data_scadenza) {
                if (prec_lotto == NULL) {
                    //se il lotto da eliminare è il primo della lista
                    if (current->quantita == 0) {
                        //se la quantità è 0 elimino il lotto
                        magazzino.lotti[hash] = lotto->next;
                        free(lotto);
                    } else
                        lotto->quantita = current->quantita; //altrimenti aggiorno la quantità
                } else {
                    if (current->quantita == 0) {
                        //se la quantità è 0 elimino il lotto
                        prec_lotto->next = lotto->next;
                        free(lotto);
                    } else
                        lotto->quantita = current->quantita; //altrimenti aggiorno la quantità
                }
                break;
            }
            prec_lotto = lotto;
            lotto = lotto->next;
        }
        current = current->next;
    }
}

//cerca un ordine da una lista concatenata
int cercaOrdine(Ordine *ordine, Ordine *listaOrdini) {
    Ordine *current = listaOrdini;
    while (current != NULL) {
        if (current == ordine)
            return 1; //se è già presente
        current = current->next;
    }
    return 0; //se non è presente
}

//elimina ordine dalla lista degli ordini pronti
void eliminaOrdinePronto(Ordine *ordine) {
    Ordine *current = magazzino.ordini_pronti;
    Ordine *prec = NULL;

    while (current != NULL) {
        if (current == ordine) {
            if (prec == NULL) {
                //se l'ordine da eliminare è il primo della lista
                magazzino.ordini_pronti = magazzino.ordini_pronti->next;
            } else {
                //se l'ordine da eliminare è in mezzo alla lista
                prec->next = current->next;
            }

            //eliminaRicettaDaOrdine(ordine);
            //free(current);
            return;
        }
        prec = current;
        current = current->next;
    }
}

void eliminaOrdineAttesa(Ordine *ordine) {
    Ordine *current = magazzino.ordini_attesa;
    Ordine *prec = NULL;

    while (current != NULL) {
        if (current == ordine) {
            if (prec == NULL) {
                //se l'ordine da eliminare è il primo della lista
                magazzino.ordini_attesa = magazzino.ordini_attesa->next;
            } else {
                //se l'ordine da eliminare è in mezzo alla lista
                prec->next = current->next;
            }

            //eliminaRicettaDaOrdine(ordine);
            //free(current);
            return;
        }
        prec = current;
        current = current->next;
    }
}



//decide se bisogna mettere l' ordine nella lista di attesa o di pronto nel magazzino
void gestioneOrdini(Ordine *ordine) {
    //VERIFICO SE L'ORDINE PUO' ESSERE PREPARATO
    int n_ingredienti = 0, n_lotti = 0;
    int tutti_gli_ingredienti_presenti = 1;
    Lotto *lotti = NULL;
    Ingrediente *ingrediente = ordine->ricetta.ingredienti;

    while (ingrediente != NULL) {
        //ingrediente che sto cercando
        n_ingredienti++;
        int quantita = ordine->quantita; //quantità di dolci di quella ricetta

        //CERCO I LOTTI NECESSARI, SE RAGGIUNGO LA QUANTITA RICHIESTA MI FERMO
        int y = trovaLottiNecessariPerIngrediente(&lotti, ingrediente->nome_ingrediente,
                                                  quantita * ingrediente->quantita);
        if (y == 0) {
            tutti_gli_ingredienti_presenti = 0;
            break;
        }
        n_lotti++;
        ingrediente = ingrediente->next;
    }
    if (n_ingredienti == n_lotti && tutti_gli_ingredienti_presenti == 1) {
        if (cercaOrdine(ordine, magazzino.ordini_attesa) == 1)
            eliminaOrdineAttesa(ordine);
        aggiungiOrdineInPronto(ordine);
        eliminaLotti(lotti);
    } else {
        if (cercaOrdine(ordine, magazzino.ordini_attesa) == 0)
            aggiungiOrdineInAttesa(ordine);
    }
}

//carico il camion con gli ordini pronti nel magazzino fino a raggiungere la capienza massima possibile
void caricaCamion() {
    Ordine *ordine_pronto = magazzino.ordini_pronti;
    int peso_camion = 0;
    while (ordine_pronto != NULL) {
        //aggiungi l'ordine al camion se c'è spazio
        if ((corriere.capienza_camion-peso_camion)>=ordine_pronto->peso) {
            peso_camion += ordine_pronto->peso;
            aggiungiOrdineAlCorriere(ordine_pronto);
            eliminaOrdinePronto(ordine_pronto);
        }
        ordine_pronto = ordine_pronto->next;
    }
}

void scaricaMerce() {
    //elimina gli ordini dal camion
    Ordine *ordine = corriere.ordini;
    while (ordine != NULL) {
        Ordine *temp = ordine;
        ordine = ordine->next;
        free(temp);
    }
    corriere.ordini = NULL;
}


// Rimuove il newline alla fine della stringa e verifica se contiene '\a'
void rimuoviNewLine(char *line) {
    size_t len = strlen(line);
    // Controlla se l'ultimo carattere è '\n' e rimuovilo
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
}

int main(void) {
    inizializzaCatalogo();
    inizializzaCorriere();
    inizializzaMagazzino();


    int contatore_linee = -1;

    // Variabili per la gestione della lettura
    char *line = NULL;
    size_t len = 0;

    // Legge lo standard input riga per riga
    while (getline(&line, &len, stdin) != -1) {
        rimuoviNewLine(line);

        // Tokenizza la stringa
        char *token = strtok(line, " ");
        //STAMPA CAMIONCINO ogni k-periodicita
        //CALCOLO PERIODICITA E CAPIENZA CAMION
        if (contatore_linee == -1) {
            if (token != NULL) {
                corriere.periodicita = atoi(token);
                token = strtok(NULL, " ");
            } else {
                fprintf(stderr, "Manca la periodicita \n");
                free(line);
                continue;
            }
            if (token != NULL) {
                corriere.capienza_camion = atoi(token);
            } else {
                fprintf(stderr, "Manca la capienza \n");
                free(line);
                continue;
            }
        } else {

            if (contatore_linee % corriere.periodicita == 0 && contatore_linee!=0) {
                caricaCamion();
                stampaCamion();
                scaricaMerce();
            }

            if (token != NULL) {
                //AGGIUNGI RICETTA
                if (strcmp(token, "aggiungi_ricetta") == 0) {
                    Ricetta *nuova_ricetta = malloc(sizeof(Ricetta));

                    if (nuova_ricetta == NULL) {
                        fprintf(stderr, "Errore di allocazione della memoria per la ricetta!\n");
                        free(line);
                        exit(EXIT_FAILURE);
                    }

                    token = strtok(NULL, " "); // Nome ricetta
                    if (token != NULL) {
                        inizializzaRicetta(nuova_ricetta, token);
                    } else {
                        fprintf(stderr, "Nome ricetta mancante\n");
                        free(nuova_ricetta);
                        free(line);
                        continue;
                    }

                    while ((token = strtok(NULL, " ")) != NULL) {
                        char nome_ingrediente[255];
                        int quantita_ingrediente;

                        //salva il nome dell'ingrediente
                        strncpy(nome_ingrediente, token, sizeof(nome_ingrediente));
                        nome_ingrediente[sizeof(nome_ingrediente) - 1] = '\0';

                        //salva la quantità dell'ingrediente
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                            quantita_ingrediente = atoi(token);
                        } else {
                            fprintf(stderr, "Quantità ingrediente mancante\n");
                            continue;
                        }
                        aggiungiIngredienteAllaRicetta(nuova_ricetta, nome_ingrediente, quantita_ingrediente);
                    }

                    aggiungiRicettaAlCatalogo(nuova_ricetta);
                }

                //RIMUOVI RICETTA
                else if (strcmp(token, "rimuovi_ricetta") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        rimuoviRicetta(token);
                    }
                }

                //RIFORNIMENTO (AGGIUNGI LOTTO)
                else if (strcmp(token, "rifornimento") == 0) {
                    while ((token = strtok(NULL, " ")) != NULL) {
                        char nome_ingrediente[255];
                        int quantita_ingrediente;
                        int data_scadenza;

                        //salva il nome dell'ingrediente
                        strncpy(nome_ingrediente, token, sizeof(nome_ingrediente));
                        nome_ingrediente[sizeof(nome_ingrediente) - 1] = '\0';

                        //salva la quantità dell'ingrediente
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                            quantita_ingrediente = atoi(token);
                        } else {
                            fprintf(stderr, "Quantità ingrediente mancante\n");
                            continue;
                        }
                        //salva la data di scadenza dell'ingrediente
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                            data_scadenza = atoi(token);
                        } else {
                            fprintf(stderr, "Data di scadenza mancante\n");
                            continue;
                        }
                        aggiungiLottoAlMagazzino(nome_ingrediente, quantita_ingrediente, data_scadenza);

                        //verifico se posso soddisfare gli ordini in attesa
                        if (magazzino.ordini_attesa != NULL) {
                            Ordine *lista = magazzino.ordini_attesa;
                            while (lista != NULL) {
                                Ordine *next = lista->next;
                                gestioneOrdini(lista);
                                lista = next;
                            }
                        }
                    }
                    printf("rifornito\n");
                }

                //ORDINE
                else if (strcmp(token, "ordine") == 0) {
                    Ordine *nuovo_ordine = malloc(sizeof(Ordine));

                    //salva nome ricetta
                    char nome_ricetta[255];
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        strncpy(nome_ricetta, token, sizeof(nome_ricetta));
                        nome_ricetta[sizeof(nome_ricetta) - 1] = '\0';
                    } else {
                        fprintf(stderr, "Nome ricetta mancante\n");
                        free(nuovo_ordine);
                        free(line);
                        continue;
                    }
                    //salva quantita
                    int quantita;
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        quantita = atoi(token);
                    } else {
                        fprintf(stderr, "Quantità mancante\n");
                        free(nuovo_ordine);
                        free(line);
                        continue;
                    }
                    nuovo_ordine = creaOrdine(nome_ricetta, quantita, contatore_linee);
                    if (nuovo_ordine != NULL)
                        gestioneOrdini(nuovo_ordine);
                } else {
                    printf("Comando non riconosciuto\n");
                }
            }
        }
        contatore_linee++;
    }
    free(line);
    //fai la free del magazzino, catalogo e corriere
    return 0;
}
