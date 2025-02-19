#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define TABELLA_SIZE 5639

typedef struct Ingrediente {
    char *nome_ingrediente;
    u_int32_t quantita;
    u_int32_t hash;
    struct Ingrediente *next;
} Ingrediente;

typedef struct Ricetta {
    char *nome_ricetta;
    Ingrediente *ingredienti; // lista concatenata di ingredienti
    u_int32_t peso; //somma dei grammi dei singoli ingredienti
    u_int32_t hash;
    u_int32_t last_quantita_fallita;
    u_int32_t last_tempo_fallita;
    struct Ricetta *next; // Per la lista concatenata di ricette

} Ricetta;

typedef struct {
   Ricetta *ricette[TABELLA_SIZE];
} Catalogo;

typedef struct Lotto {
    char *nome_ingrediente;
    u_int32_t quantita;
    u_int32_t data_scadenza;
    struct Lotto *next;
} Lotto;

typedef struct Ordine {
    u_int32_t istante_arrivo;
    u_int32_t quantita; //quantità di quella ricetta
    Ricetta *ricetta;
    struct Ordine *next;
    u_int32_t peso; // somma degli ingredienti della ricetta * quantità
} Ordine;

typedef struct ListaLotti {
    Lotto *lottiElemento; //lista di un singolo ingrediente
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
unsigned int last_rifornimento=0;

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
//cerca una ricetta all' interno di un ordine in magazzino
int cercaRicetta(char *nome) {
    unsigned int hash = hash_ricetta(nome);
    Ricetta *ricetta = catalogo.ricette[hash];

    while (ricetta != NULL) {
        if (strcmp(ricetta->nome_ricetta, nome) == 0) {
            Ordine *ordine = magazzino.ordini_attesa;
            while (ordine != NULL) {
                if (ordine->ricetta == ricetta) {
                    return 1; // Ricetta trovata negli ordini in attesa
                }
                ordine = ordine->next;
            }
            ordine = magazzino.ordini_pronti;
            while (ordine != NULL) {
                if (ordine->ricetta == ricetta) {
                    return 1; // Ricetta trovata negli ordini pronti
                }
                ordine = ordine->next;
            }
        }
        ricetta = ricetta->next;
    }
    return 0; // Ricetta non trovata
}

//serve per eliminare gli ingredienti di una ricetta che devo eliminare
void liberaIngredientiDiUnaRicetta(Ingrediente *ingredienti) {
    //Ingrediente *ingrediente = r->ingredienti;
    while (ingredienti != NULL) {
        //scorro tutta la lista concatenata di ingredienti
        Ingrediente *temp = ingredienti; //salvo il puntatore all'ingrediente corrente
        ingredienti = ingredienti->next;
        free(temp->nome_ingrediente);//passo all'ingrediente successivo
        free(temp); //libero la memoria dell'ingrediente corrente
    }
}

// Funzione per liberare la memoria di una ricetta
void liberaRicetta(Ricetta *r) {
    if (r != NULL) {
        free(r->nome_ricetta);
        liberaIngredientiDiUnaRicetta(r->ingredienti);
        free(r);
    }
}

// Rimuove una ricetta dal catalogo
void rimuoviRicetta(char *nome_ricetta) {
    unsigned int hash = hash_ricetta(nome_ricetta);
    Ricetta *ricetta = catalogo.ricette[hash];
    Ricetta *ricetta_prec = NULL;
    while (ricetta != NULL) {
        if (cercaRicetta(nome_ricetta) == 1) {
            printf("ordini in sospeso\n");
            return;
        }
        if (strcmp(nome_ricetta, ricetta->nome_ricetta) == 0) {
            if (ricetta_prec == NULL) {
                catalogo.ricette[hash] = ricetta->next;
            } else {
                ricetta_prec->next = ricetta->next;
            }
            liberaRicetta(ricetta); // libera la ricetta e i suoi ingredienti
            printf("rimossa\n");
            return;
        }
        ricetta_prec = ricetta;
        ricetta = ricetta->next;
    }
    printf("non presente\n");
}

//per creare la ricetta gli metto la lista concatenata degli ingredienti
void aggiungiIngredienteAllaRicetta(Ricetta *ricetta, char *nome_ingrediente, int quantita) {
    //CREA NUOVO INGREDIENTE
    Ingrediente *nuovo_ingrediente = malloc(sizeof(Ingrediente));
    nuovo_ingrediente->nome_ingrediente= nome_ingrediente; //copio il nome dell'ingrediente
    nuovo_ingrediente->quantita = quantita; //imposto la quantità
    nuovo_ingrediente->hash = hash_ingrediente(nome_ingrediente); //imposto l'hash

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
    nuovo_lotto->nome_ingrediente = nome_ingrediente;
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
                    free(nuovo_lotto->nome_ingrediente);
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
    nuovo->lottiElemento = nuovo_lotto;
    nuovo->next = magazzino.lotti[hash];
    magazzino.lotti[hash] = nuovo;
}

//aggiungo la ricetta al catalogo
void aggiungiRicettaAlCatalogo(Ricetta *nuova_ricetta) {
    unsigned int hash = nuova_ricetta->hash;
    Ricetta **ricetta = &catalogo.ricette[hash];

    while (*ricetta != NULL) {
        if (strcmp(nuova_ricetta->nome_ricetta, (*ricetta)->nome_ricetta) == 0) {
            liberaIngredientiDiUnaRicetta(nuova_ricetta->ingredienti);
            free(nuova_ricetta->nome_ricetta);
            free(nuova_ricetta);
            printf("ignorato\n");
            return;
        }
        ricetta = &(*ricetta)->next;
    }

    nuova_ricetta->next = catalogo.ricette[hash];
    catalogo.ricette[hash] = nuova_ricetta;
    printf("aggiunta\n");
}

//creo un ordine
Ordine *creaOrdine(char *nome_ricetta, int quantita) {
    //cerca la ricetta nel catalogo
    unsigned int hash = hash_ricetta(nome_ricetta);
    Ricetta *ricetta = catalogo.ricette[hash];

    while (ricetta != NULL) {
        if (strcmp(nome_ricetta, ricetta->nome_ricetta) == 0) {
            Ordine *ordine = malloc(sizeof(Ordine));
            ordine->ricetta = ricetta;
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
    r->nome_ricetta = nome_ricetta;
    r->next = NULL;
    r->peso = 0;
    r->ingredienti = NULL;
    r->hash = hash_ricetta(nome_ricetta);
    r->last_quantita_fallita = 0;
    r->last_tempo_fallita = 0;
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

//serve per stampare il contenuto del camion oppure dire se è vuoto
void stampaCamion() {
    if (corriere.ordini == NULL) {
        printf("camioncino vuoto\n");
        return;
    }
    for (Ordine *ordine = corriere.ordini; ordine != NULL; ordine = ordine->next) {
        printf("%d %s %d\n", ordine->istante_arrivo, ordine->ricetta->nome_ricetta, ordine->quantita);
    }
}

//aggiungo in lista di pronto un ordine a seconda dell' istante di arrivo
void aggiungiOrdineInPronto(Ordine *ordine) {
    ordine->next = NULL;

    if (magazzino.ordini_pronti == NULL || magazzino.ordini_pronti->istante_arrivo > ordine->istante_arrivo) {
        ordine->next = magazzino.ordini_pronti;
        magazzino.ordini_pronti = ordine;
    } else {
        Ordine *current = magazzino.ordini_pronti;
        while (current->next != NULL && current->next->istante_arrivo <= ordine->istante_arrivo) {
            current = current->next;
        }
        ordine->next = current->next;
        current->next = ordine;
    }
}

//aggiungo al corriere un ordine a seconda dell' istante di arrivo
void aggiungiOrdineAlCorriere(Ordine *ordine) {
    Ordine **current = &magazzino.ordini_pronti;

    while (*current != NULL) {
        if (*current == ordine) {
            *current = ordine->next;
            break;
        }
        current = &(*current)->next;
    }

    ordine->next = NULL;
    current = &corriere.ordini;

    while (*current != NULL && (*current)->peso >= ordine->peso) {
        current = &(*current)->next;
    }

    ordine->next = *current;
    *current = ordine;
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
            return;
        }
        prec = current;
        current = current->next;
    }
}

//restituisce il lotto con la scadenza minore di uno specifico ingrediente
int trovaLottiNecessariPerIngrediente(Ingrediente *ingrediente, int quantita_necessaria) {
    uint32_t tot_trovato = 0;
    ListaLotti *lottiIngrediente = magazzino.lotti[ingrediente->hash];
    ListaLotti *next = NULL;
    ListaLotti *precLista = NULL;

    while(lottiIngrediente!=NULL) {
        next = lottiIngrediente->next;
        if (strcmp(ingrediente->nome_ingrediente, lottiIngrediente->lottiElemento->nome_ingrediente) == 0) {
            Lotto *lotto_magazzino = lottiIngrediente->lottiElemento;
            Lotto *prec = NULL;
            while (lotto_magazzino != NULL && tot_trovato < quantita_necessaria) {
                //ELIMINA LOTTI SCADUTI
                if (lotto_magazzino->data_scadenza <= contatore_linee) {
                    if (prec == NULL) {//se è il primo della lista
                        lottiIngrediente->lottiElemento = lotto_magazzino->next;
                    } else {// se è in mezzo alla lista
                        prec->next = lotto_magazzino->next;
                    }
                    Lotto *da_eliminare = lotto_magazzino;
                    lotto_magazzino = lotto_magazzino->next;
                    free(da_eliminare->nome_ingrediente);
                    free(da_eliminare);
                    continue;
                }

                if(lotto_magazzino->quantita >= quantita_necessaria-tot_trovato){
                    return 1;
                }

                tot_trovato += lotto_magazzino->quantita;
                prec = lotto_magazzino;
                lotto_magazzino = lotto_magazzino->next;
            }
            // Se non ci sono più lotti nella lista, elimina la lista
            if (lottiIngrediente->lottiElemento == NULL) {
                if (precLista == NULL) {
                    magazzino.lotti[ingrediente->hash] = lottiIngrediente->next;
                } else {
                    precLista->next = lottiIngrediente->next; // Salta l'elemento
                }
                free(lottiIngrediente->lottiElemento);
                free(lottiIngrediente);
            }
        }
        precLista = lottiIngrediente;
        lottiIngrediente = next;
    }
    return 0;
}

void eliminaLotti(Ordine *ordine) {
    Ingrediente *ingrediente = ordine->ricetta->ingredienti;

    while (ingrediente != NULL) {
        ListaLotti *lottiIngrediente = magazzino.lotti[ingrediente->hash];
        ListaLotti *precLista = NULL;

        while (lottiIngrediente != NULL) {
            if (strcmp(ingrediente->nome_ingrediente, lottiIngrediente->lottiElemento->nome_ingrediente) == 0) {
                Lotto *lotto_magazzino = lottiIngrediente->lottiElemento;
                uint32_t quantita_necessaria = ordine->quantita * ingrediente->quantita;
                uint32_t quantita_trovata = 0;

                while (lotto_magazzino != NULL && quantita_trovata < quantita_necessaria) {
                    if (lotto_magazzino->quantita > quantita_necessaria - quantita_trovata) {
                        lotto_magazzino->quantita -= quantita_necessaria - quantita_trovata;
                        quantita_trovata = quantita_necessaria;
                    } else {
                        quantita_trovata += lotto_magazzino->quantita;
                        Lotto *da_eliminare = lotto_magazzino;
                        lotto_magazzino = lotto_magazzino->next;
                        free(da_eliminare->nome_ingrediente);
                        free(da_eliminare);
                    }
                }

                lottiIngrediente->lottiElemento = lotto_magazzino;
                if (lottiIngrediente->lottiElemento == NULL) {
                    if (precLista == NULL) {
                        magazzino.lotti[ingrediente->hash] = lottiIngrediente->next;
                    } else {
                        precLista->next = lottiIngrediente->next;
                    }
                    free(lottiIngrediente);
                }
                break;
            }
            precLista = lottiIngrediente;
            lottiIngrediente = lottiIngrediente->next;
        }
        ingrediente = ingrediente->next;
    }
}

//decide se bisogna mettere un ordine in attesa in pronto
void gestioneOrdiniInAttesa(Ordine *ordine) {
    //VERIFICO SE L'ORDINE PUO' ESSERE PREPARATO
    Ingrediente *ingrediente = ordine->ricetta->ingredienti;

    while (ingrediente != NULL) {
        int quantita_necessaria = ordine->quantita * ingrediente->quantita; //quantità di dolci di quella ricetta

        //CERCO I LOTTI NECESSARI, SE RAGGIUNGO LA QUANTITA RICHIESTA MI FERMO
        if (trovaLottiNecessariPerIngrediente(ingrediente, quantita_necessaria)==0) {
            ordine->ricetta->last_quantita_fallita = ordine->quantita;
            ordine->ricetta->last_tempo_fallita = contatore_linee;
            return;
        }
        ingrediente = ingrediente->next;
    }
    eliminaOrdineAttesa(ordine);
    aggiungiOrdineInPronto(ordine);
    eliminaLotti(ordine);
}

//decide se bisogna mettere nuovo ordine nella lista di attesa o di pronto nel magazzino
void gestioneOrdineNuovo(Ordine *ordine) {
    //VERIFICO SE L'ORDINE PUO' ESSERE PREPARATO
    int tutti_gli_ingredienti_presenti = 1;
    Ingrediente *ingrediente = ordine->ricetta->ingredienti;

    while (ingrediente != NULL) {
        int quantita_necessaria = ordine->quantita * ingrediente->quantita; //quantità di dolci di quella ricetta

        //CERCO I LOTTI NECESSARI, SE RAGGIUNGO LA QUANTITA RICHIESTA MI FERMO
        if (trovaLottiNecessariPerIngrediente(ingrediente, quantita_necessaria) == 0) {
            if (ordine->ricetta->last_quantita_fallita > ordine->quantita)
                ordine->ricetta->last_quantita_fallita = ordine->quantita;
            ordine->ricetta->last_tempo_fallita = contatore_linee;
            tutti_gli_ingredienti_presenti = 0;
            break;
        }
        ingrediente = ingrediente->next;
    }
    if (tutti_gli_ingredienti_presenti == 1) {
        aggiungiOrdineInPronto(ordine);
        eliminaLotti(ordine);
    } else {
        if (magazzino.ordini_attesa == NULL) {
            magazzino.ordini_attesa = ordine;
        } else {
            ultimo_ordine_attesa->next = ordine;
        }
        ultimo_ordine_attesa = ordine;
    }
}

//carico il camion con gli ordini pronti nel magazzino fino a raggiungere la capienza massima possibile
void caricaCamion() {
    Ordine *ordine_pronto = magazzino.ordini_pronti;
    uint32_t peso_camion = 0;
    Ordine *next = NULL;
    while (ordine_pronto != NULL) {
        next = ordine_pronto->next;
        //aggiungi l'ordine al camion se c'è spazio
        if ((corriere.capienza_camion - peso_camion) >= ordine_pronto->peso) {
            peso_camion += ordine_pronto->peso;
            aggiungiOrdineAlCorriere(ordine_pronto);
        } else
            break;
        ordine_pronto = next;
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

    //Calcola periodicita
    char *token = strtok(line, " ");
    corriere.periodicita = atoi(token);

    //Calcola capienza
    //CALCOLO PERIODICITA E CAPIENZA CAMION
    token = strtok(NULL, " ");
    corriere.capienza_camion = atoi(token);

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

                // Nome ricetta
                token = strtok(NULL, " ");
                char *nome_ricetta = malloc(strlen(token)+1);
                strcpy(nome_ricetta, token);

                inizializzaRicetta(nuova_ricetta, nome_ricetta);

                while ((token = strtok(NULL, " ")) != NULL) {
                    //salva il nome dell'ingrediente
                    char *nome_ingrediente=malloc((strlen(token)+1));
                    strcpy(nome_ingrediente, token);

                    //salva la quantità dell'ingrediente
                    token = strtok(NULL, " ");
                    int quantita_ingrediente = atoi(token);

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
                last_rifornimento=contatore_linee;
                while ((token = strtok(NULL, " ")) != NULL) {
                    
                    //salva il nome dell'ingrediente
                    char *nome_ingrediente = malloc(sizeof(char)*(strlen(token)+1));
                    strcpy(nome_ingrediente, token);

                    //salva la quantità dell'ingrediente
                    token = strtok(NULL, " ");
                    int quantita_ingrediente = atoi(token);

                    //salva la data di scadenza dell'ingrediente
                    token = strtok(NULL, " ");
                    int data_scadenza = atoi(token);

                    aggiungiLottoAlMagazzino(nome_ingrediente, quantita_ingrediente, data_scadenza);
                }
                printf("rifornito\n");
                //verifico se posso soddisfare gli ordini in attesa
                if (magazzino.ordini_attesa != NULL) {
                    Ordine *lista = magazzino.ordini_attesa;
                    while (lista != NULL) {
                        Ordine *next = lista->next;

                        //verifico l'ordine ha una quantità maggiore rispetto a un precedente ordine fallito per la quantità della ricetta
                        if (contatore_linee != lista->ricetta->last_tempo_fallita || lista->quantita < lista->ricetta->last_quantita_fallita) {
                            gestioneOrdiniInAttesa(lista);
                        }
                        lista = next;
                    }
                }
            }

            //ORDINE
            else if (strcmp(token, "ordine") == 0) {
                //salva nome ricetta
                token = strtok(NULL, " ");
                char *nome_ricetta= malloc(sizeof(char)*(strlen(token)+1));
                strcpy(nome_ricetta, token);

                //salva quantita
                token = strtok(NULL, " ");
                int quantita;
                quantita = atoi(token);

                Ordine *nuovo_ordine = creaOrdine(nome_ricetta, quantita);
                if (nuovo_ordine != NULL)
                    gestioneOrdineNuovo(nuovo_ordine);
                free(nome_ricetta);
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