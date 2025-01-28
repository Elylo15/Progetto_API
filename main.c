#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define TABELLA_SIZE 10007

typedef struct Ingrediente {
    char nome_ingrediente[255];
    u_int32_t quantita;
    u_int32_t hash;
    struct Ingrediente *next;
} Ingrediente;

typedef struct Ricetta {
    char nome_ricetta[255];
    Ingrediente *ingredienti; // lista concatenata di ingredienti
    u_int32_t peso; //somma dei grammi dei singoli ingredienti
    u_int32_t hash;
    struct Ricetta *next; // Per la lista concatenata di ricette

} Ricetta;

typedef struct {
   Ricetta *ricette[TABELLA_SIZE];
} Catalogo;

typedef struct Lotto {
    char nome_ingrediente[255];
    u_int32_t quantita;
    u_int32_t data_scadenza;
    struct Lotto *next;
} Lotto;

typedef struct Ordine {
    u_int32_t istante_arrivo;
    u_int32_t quantita; //quantità di quella ricetta
    Ricetta ricetta;
    struct Ordine *next;
    u_int32_t peso; // somma degli ingredienti della ricetta * quantità
} Ordine;

typedef struct ListaLotti {
    Lotto *lottiElemento;
    struct ListaLotti *next;
} ListaLotti;

typedef struct {
    ListaLotti *lotti[TABELLA_SIZE];
    Ordine *ordini_attesa; //lista concatenata di ordini in attesa
    Ordine *ordini_pronti; //lista concatenata di ordini pronti
} Magazzino;


typedef struct {
    u_int32_t capienza_camion;
    u_int32_t periodicita;
    Ordine *ordini;
} Corriere;

//VARIIABILI GLOBALI
Catalogo catalogo;
Corriere corriere;
Magazzino magazzino;
unsigned int contatore_linee = 0;
Ordine *ultimo_ordine_attesa = NULL;

//mettere unsigned per diminuire lo spazio
//per le strut usare packed per diminuire lo spazio


//FUNZIONE DI HASH
unsigned int murmurhash(const char *key, unsigned int len) {
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
    //imposto l'hash
    nuovo_ingrediente->hash = hash_ingrediente(nome_ingrediente);


    //aggiungo l'ingrediente alla lista concatenata
    nuovo_ingrediente->next = ricetta->ingredienti;
    ricetta->ingredienti = nuovo_ingrediente;
    ricetta->peso += quantita;
}

//creo il lotto e lo aggiungo al magazzino in ordine di data di scadenza
void aggiungiLottoAlMagazzino(char *nome_ingrediente, int quantita, int data_scadenza) {
    u_int32_t hash = hash_ingrediente(nome_ingrediente);

    //CREA NUOVO LOTTO
    Lotto *nuovo_lotto = malloc(sizeof(Lotto));
    if (nuovo_lotto == NULL) {
        fprintf(stderr, "Errore di allocazione delula memoria!\n");
        exit(EXIT_FAILURE);
    }
    strncpy(nuovo_lotto->nome_ingrediente, nome_ingrediente, 254);
    nuovo_lotto->nome_ingrediente[254] = '\0';
    nuovo_lotto->quantita = quantita;
    nuovo_lotto->data_scadenza = data_scadenza;
    nuovo_lotto->next = NULL;

    //aggiungo il lotto all'array di lotti in base alla data di scadenza
    ListaLotti *current = magazzino.lotti[hash];
    while (current != NULL) {
        if(strcmp(current->lottiElemento->nome_ingrediente, nome_ingrediente)==0) {//se è già presente un lotto di quell'ingrediente

            //vado dentro alla lista di quel ingrediente
            Lotto *lottoIngrediente = current->lottiElemento;
            Lotto *prec = NULL;

            while (lottoIngrediente!=NULL) {
                if(lottoIngrediente->data_scadenza> data_scadenza){
                    if (prec == NULL) {
                        //se il lotto da aggiungere va all'inizio
                        nuovo_lotto->next = lottoIngrediente;
                        current->lottiElemento = nuovo_lotto;
                    } else {
                        //se il lotto da aggiungere è in mezzo alla lista
                        prec->next = nuovo_lotto;
                        nuovo_lotto->next = lottoIngrediente;
                    }
                    return;
                }
                if (lottoIngrediente->data_scadenza == data_scadenza) {
                    lottoIngrediente->quantita += quantita;
                    free(nuovo_lotto);
                    return;
                }
                prec = lottoIngrediente;
                lottoIngrediente = lottoIngrediente->next;
            }
            // Se arriva alla fine della lista senza trovare una data maggiore
            if (prec!=NULL) {
                prec->next = nuovo_lotto;
                return;
            }
        }
        current = current->next;
    }

    // Se non esiste una lista per questo ingrediente, crea una nuova ListaLotti
    ListaLotti *nuovo = malloc(sizeof(ListaLotti));
    if (nuovo == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria!\n");
        exit(EXIT_FAILURE);
    }
    nuovo->lottiElemento = nuovo_lotto;
    nuovo->next = magazzino.lotti[hash];
    magazzino.lotti[hash] = nuovo;
}

//aggiungo la ricetta al catalogo
void aggiungiRicettaAlCatalogo(Ricetta *nuova_ricetta) {
    // Cerca se la ricetta è già presente nella lista concatenata all'indice hash
    Ricetta *ricetta = catalogo.ricette[nuova_ricetta->hash];
    while (ricetta != NULL) {
        if (strcmp(nuova_ricetta->nome_ricetta, ricetta->nome_ricetta) == 0) {
            // Ricetta già presente, non aggiungere
            printf("ignorato\n");
            return;
        }
        ricetta = ricetta->next;
    }

    // Se non trovata, aggiungi la nuova ricetta
    nuova_ricetta->next = catalogo.ricette[nuova_ricetta->hash];
    catalogo.ricette[nuova_ricetta->hash] = nuova_ricetta;
    printf("aggiunta\n");
}

//creo un ordine
Ordine *creaOrdine(char *nome_ricetta, int quantita) {
    //cerca la ricetta nel catalogo
    Ordine *ordine = malloc(sizeof(Ordine));
    unsigned int hash = hash_ricetta(nome_ricetta);
    Ricetta *ricetta = catalogo.ricette[hash];
    while (ricetta != NULL) {
        if (strcmp(nome_ricetta, ricetta->nome_ricetta) == 0) {
            ordine->ricetta = *ricetta;
            ordine->quantita = quantita;
            ordine->istante_arrivo = contatore_linee;
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
    r->ingredienti = NULL;
    r->hash = hash_ricetta(nome_ricetta);
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

//cerca una ricetta all' interno di un odine in magazzino
int cercaRicetta(char *nome) {
    unsigned int hash = hash_ricetta(nome);
    Ricetta *ricetta = catalogo.ricette[hash];
    Ordine *ordini_attesa = magazzino.ordini_attesa;
    Ordine *ordini_pronti = magazzino.ordini_pronti;


    while (ricetta != NULL) {
        if (strcmp(ricetta->nome_ricetta, nome) == 0) {
            while (ordini_attesa != NULL) {
                if (strcmp(ordini_attesa->ricetta.nome_ricetta, nome) == 0) {
                    return 1; //questa ricetta è in
                }
                ordini_attesa = ordini_attesa->next;
            }
            while (ordini_pronti != NULL) {
                if (strcmp(ordini_pronti->ricetta.nome_ricetta, nome) == 0) {
                    return 1; //questa ricetta è in
                }
                ordini_pronti = ordini_pronti->next;
            }
        }

        ricetta = ricetta->next;
    }
    return 0; //se la ricetta non è usata ne dagli ordini pronti che in attesa
}

//serve per eliminare gli ingredienti di una ricetta che devo eliminare
void liberaIngredientiDiUnaRicetta(Ricetta *r) {
    if (r == NULL)
        return;

    Ingrediente *ingrediente = r->ingredienti;
    while (ingrediente != NULL) {
        //scorro tutta la lista concatenata di ingredienti
        Ingrediente *temp = ingrediente; //salvo il puntatore all'ingrediente corrente
        ingrediente = ingrediente->next; //passo all'ingrediente successivo
        free(temp); //libero la memoria dell'ingrediente corrente
    }

    r->ingredienti = NULL;
}

//rimuove una ricetta dal catalogo
void rimuoviRicetta(char *nome_ricetta) {
    unsigned int hash = hash_ricetta(nome_ricetta);

    // Cerca se la ricetta è presente nella lista concatenata all'indice hash
    Ricetta *ricetta = catalogo.ricette[hash];
    Ricetta *ricetta_prec = NULL;
    while (ricetta != NULL) {
        if (cercaRicetta(nome_ricetta) == 1) {
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
            printf("rimossa\n");
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

//aggiungo in lista di pronto un ordine a seconda dell' istante di arrivo
void aggiungiOrdineInPronto(Ordine *ordine) {
    ordine->next = NULL;

    // Se la lista è vuota o se l'ordine deve essere inserito all'inizio
    if (magazzino.ordini_pronti == NULL) {
        magazzino.ordini_pronti = ordine;
    } else {
        Ordine *current = magazzino.ordini_pronti;
        //se il primo ha un istante di arrivo maggiore di quello da aggiungere
        if (magazzino.ordini_pronti->istante_arrivo > ordine->istante_arrivo) {
            ordine->next = magazzino.ordini_pronti;
            magazzino.ordini_pronti = ordine;
        } else {
            while (current->next != NULL && current->next->istante_arrivo <= ordine->istante_arrivo) {
                current = current->next;
            }
            // Inserisci il nuovo ordine nella posizione corretta
            ordine->next = current->next;
            current->next = ordine;
        }
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


//cerca un ordine da una lista concatenata
int cercaOrdineinAttesa(Ordine *ordine) {
    Ordine *current = magazzino.ordini_attesa;
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
            return;
        }
        prec = current;
        current = current->next;
    }
}

//elimina ordine dalla lista degli ordini attesa
void eliminaOrdineAttesa(Ordine *ordine) {
    Ordine *current = magazzino.ordini_attesa;
    Ordine *prec = NULL;

    while (current != NULL) {
        if (current == ordine) {
            if (prec == NULL) {
                //se l'ordine da eliminare è il primo della lista
                magazzino.ordini_attesa = magazzino.ordini_attesa->next;
                if (magazzino.ordini_attesa == NULL) {
                    ultimo_ordine_attesa = NULL;
                }
            } else {
                //se l'ordine da eliminare è in mezzo alla lista
                prec->next = current->next;
            }
            if (current->next == NULL) {
                ultimo_ordine_attesa = prec;
            }
            //free(current);
            return;
        }
        prec = current;
        current = current->next;
    }
}

//restituisce il lotto con la scadenza minore di uno specifico ingrediente
int trovaLottiNecessariPerIngrediente(Ingrediente *ingrediente, unsigned int quantita_necessaria) {
    uint32_t tot_trovato = 0;
    ListaLotti *lottiIngrediente = magazzino.lotti[ingrediente->hash];
    Lotto *prec = NULL;
    ListaLotti *precLista = NULL;

    while(lottiIngrediente!=NULL) {
        if (strcmp(ingrediente->nome_ingrediente, lottiIngrediente->lottiElemento->nome_ingrediente) == 0) {
            Lotto *lotto_magazzino = lottiIngrediente->lottiElemento;

            while (lotto_magazzino != NULL && tot_trovato < quantita_necessaria) {
                //ELIMINA LOTTI SCADUTI
                if (lotto_magazzino->data_scadenza <= contatore_linee) {
                    if (prec == NULL) {//se è il primo elemento della lista
                        lottiIngrediente->lottiElemento = lotto_magazzino->next;
                    } else {//se si trova in mezzo alla lista
                        prec->next = lotto_magazzino->next;
                    }
                    Lotto *da_eliminare = lotto_magazzino;
                    lotto_magazzino = lotto_magazzino->next;
                    free(da_eliminare);

                    // svuoto di la lista per mettere i nuovi lotti
                    if (lottiIngrediente->lottiElemento == NULL) {
                        if (precLista == NULL) {
                            magazzino.lotti[ingrediente->hash] = lottiIngrediente->next; // Rimuovi l'elemento della listaLotti
                        } else {
                            precLista->next = lottiIngrediente->next; // Salta l'elemento
                        }
                        free(lottiIngrediente); // Libera la memoria dell'elemento della listaLotti
                    }
                    prec = lotto_magazzino;
                    lotto_magazzino = lotto_magazzino->next;
                    continue;
                }
                if(lotto_magazzino->quantita >= quantita_necessaria-tot_trovato){
                    return 1;
                }

                tot_trovato += lotto_magazzino->quantita;

                prec = lotto_magazzino;
                lotto_magazzino = lotto_magazzino->next;
            }
        }
        precLista = lottiIngrediente;
        lottiIngrediente = lottiIngrediente->next;
    }
    return 0;
}

//elimina/modifica i lotti che appartengono a un ordine che è pronto
void eliminaLotti(Ordine *ordine) {
    //lista di ingredienti
    Ingrediente *ingrediente = ordine->ricetta.ingredienti;

    // Itera sugli ingredienti della ricetta
    while (ingrediente != NULL) {
        //lista di lotti di un ingrediente
        ListaLotti *lottiIngrediente = magazzino.lotti[ingrediente->hash];
        ListaLotti *precLista = NULL;

        // Itera sui lotti dell'ingrediente
        while (lottiIngrediente != NULL) {
            if (strcmp(ingrediente->nome_ingrediente, lottiIngrediente->lottiElemento->nome_ingrediente) == 0) {
                //prendo un elemento dei lotti
                Lotto *lotto_magazzino = lottiIngrediente->lottiElemento;
                uint32_t quantita_necessaria = ordine->quantita * ingrediente->quantita;
                uint32_t quantita_trovata = 0;

                // Itera sui lotti per ridurre la quantità necessaria
                while (lotto_magazzino != NULL && quantita_trovata < quantita_necessaria) {
                    // Se la quantità del lotto è maggiore o uguale di quella richiesta
                    if (lotto_magazzino->quantita > quantita_necessaria - quantita_trovata) {
                        lotto_magazzino->quantita -= quantita_necessaria - quantita_trovata;
                        break;
                    }

                    // Se la quantità del lotto è insufficiente, prendiamo tutto e lo eliminiamo
                    quantita_trovata += lotto_magazzino->quantita;

                    Lotto *da_eliminare = lotto_magazzino;
                    lotto_magazzino = lotto_magazzino->next;

                    lottiIngrediente->lottiElemento = lotto_magazzino;

                    free(da_eliminare);
                }

                // Se non ci sono più lotti, rimuovi l'elemento della lista
                if (lottiIngrediente->lottiElemento == NULL) {
                    if (precLista == NULL) {//se è il primo elemento della lista delle liste
                        magazzino.lotti[ingrediente->hash] = lottiIngrediente->next;
                    } else {//se si trova in mezzo alla lista
                        precLista->next = lottiIngrediente->next;
                    }
                    free(lottiIngrediente->lottiElemento);
                    free(lottiIngrediente);
                }
            }
            // Aggiorna il puntatore precLista solo se stiamo continuando
            precLista = lottiIngrediente;
            lottiIngrediente = lottiIngrediente->next; // Passa al lotto successivo
        }

        // Passa all'ingrediente successivo
        ingrediente = ingrediente->next;
    }
}

//decide se bisogna mettere nuovo ordine nella lista di attesa o di pronto nel magazzino
void gestioneOrdineNuovo(Ordine *ordine) {
    //VERIFICO SE L'ORDINE PUO' ESSERE PREPARATO
    int n_ingredienti = 0, n_lotti = 0;
    int tutti_gli_ingredienti_presenti = 1;
    Ingrediente *ingrediente = ordine->ricetta.ingredienti;

    while (ingrediente != NULL) {
        //ingrediente che sto cercando
        n_ingredienti++;
        unsigned int quantita = ordine->quantita; //quantità di dolci di quella ricetta

        //CERCO I LOTTI NECESSARI, SE RAGGIUNGO LA QUANTITA RICHIESTA MI FERMO
        int y = trovaLottiNecessariPerIngrediente(ingrediente, quantita * ingrediente->quantita);
        if (y == 0) {
            tutti_gli_ingredienti_presenti = 0;
            break;
        }
        n_lotti++;
        ingrediente = ingrediente->next;
    }
    if (n_ingredienti == n_lotti && tutti_gli_ingredienti_presenti == 1) {
        aggiungiOrdineInPronto(ordine);
        eliminaLotti(ordine);
    } else {
        if (magazzino.ordini_attesa == NULL) {
            magazzino.ordini_attesa = ordine;
            ultimo_ordine_attesa = ordine;
        } else {
            ultimo_ordine_attesa->next = ordine;
            ultimo_ordine_attesa = ordine;
        }

    }
}
//decide se bisogna mettere un ordine in attesa in pronto
void gestioneOrdiniInAttesa(Ordine *ordine) {
    //VERIFICO SE L'ORDINE PUO' ESSERE PREPARATO
    int n_ingredienti = 0, n_lotti = 0;
    int tutti_gli_ingredienti_presenti = 1;
    Ingrediente *ingrediente = ordine->ricetta.ingredienti;

    while (ingrediente != NULL) {
        //ingrediente che sto cercando
        n_ingredienti++;
         unsigned int quantita = ordine->quantita; //quantità di dolci di quella ricetta

        //CERCO I LOTTI NECESSARI, SE RAGGIUNGO LA QUANTITA RICHIESTA MI FERMO
        int y = trovaLottiNecessariPerIngrediente(ingrediente, quantita * ingrediente->quantita);
        if (y == 0) {
            tutti_gli_ingredienti_presenti = 0;
            break;
        }
        n_lotti++;
        ingrediente = ingrediente->next;
    }
    if (n_ingredienti == n_lotti && tutti_gli_ingredienti_presenti == 1) {
        eliminaOrdineAttesa(ordine);
        aggiungiOrdineInPronto(ordine);
        eliminaLotti(ordine);
    }
}

//carico il camion con gli ordini pronti nel magazzino fino a raggiungere la capienza massima possibile
void caricaCamion() {
    Ordine *ordine_pronto = magazzino.ordini_pronti;
    uint32_t peso_camion = 0;
    while (ordine_pronto != NULL) {
        //aggiungi l'ordine al camion se c'è spazio
        if ((corriere.capienza_camion - peso_camion) >= ordine_pronto->peso) {
            peso_camion += ordine_pronto->peso;
            eliminaOrdinePronto(ordine_pronto);
            aggiungiOrdineAlCorriere(ordine_pronto);
        } else
            break;
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


    // Variabili per la gestione della lettura
    char *line = NULL;
    size_t len = 0;

    len = getline(&line, &len, stdin);

    // Tokenizza la stringa
    char *token = strtok(line, " ");

    //CALCOLO PERIODICITA E CAPIENZA CAMION
    if (token != NULL) {
        corriere.periodicita = atoi(token);
        token = strtok(NULL, " ");
    } else {
        fprintf(stderr, "Manca la periodicita \n");
        free(line);
    }
    if (token != NULL) {
        corriere.capienza_camion = atoi(token);
    } else {
        fprintf(stderr, "Manca la capienza \n");
        free(line);
    }


    // Legge lo standard input riga per riga
    while (getline(&line, &len, stdin) != -1) {
        rimuoviNewLine(line);

        token = strtok(line, " ");

        //STAMPA CAMIONCINO ogni k-periodicita
        if (contatore_linee % corriere.periodicita == 0 && contatore_linee != 0) {
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
                }
                printf("rifornito\n");
                //verifico se posso soddisfare gli ordini in attesa
                if (magazzino.ordini_attesa != NULL) {
                    Ordine *lista = magazzino.ordini_attesa;
                    while (lista != NULL) {
                        Ordine *next = lista->next;
                        gestioneOrdiniInAttesa(lista);
                        lista = next;
                    }
                }
            }

            //ORDINE
            else if (strcmp(token, "ordine") == 0) {
                Ordine *nuovo_ordine;

                //salva nome ricetta
                char nome_ricetta[255];
                token = strtok(NULL, " ");
                if (token != NULL) {
                    strncpy(nome_ricetta, token, sizeof(nome_ricetta));
                    nome_ricetta[sizeof(nome_ricetta) - 1] = '\0';
                }
                //salva quantita
                int quantita;
                token = strtok(NULL, " ");
                if (token != NULL) {
                    quantita = atoi(token);
                }
                nuovo_ordine = creaOrdine(nome_ricetta, quantita);
                if (nuovo_ordine != NULL)
                    gestioneOrdineNuovo(nuovo_ordine);
            } else {
                printf("Comando non riconosciuto\n");
            }
        }
        contatore_linee++;
    }
    //STAMPA CAMIONCINO ogni k-periodicita
    if (contatore_linee % corriere.periodicita == 0 && contatore_linee != 0) {
        caricaCamion();
        stampaCamion();
        scaricaMerce();
    }
    free(line);
    return 0;
}
