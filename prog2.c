// Samuel Leal Valentin - 2023989
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
                           /* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
  };

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };

void starttimer(int AorB, float increment);
void stoptimer(int AorB);
void tolayer3(int AorB, struct pkt packet);
void tolayer5(int AorB, char datasent[20]);

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/********* BIT ALTERNANTE *********/

enum SenderState {
    WAIT_LAYER5,
    WAIT_ACK
};

struct Sender_abt {
    enum SenderState state;
    int seq;
    float estimated_rtt;
    struct pkt last_packet;
} A_abt;

struct Receiver_abt {
    int seq;
} B_abt;

int get_checksum(struct pkt *packet) {
    int checksum = 0;
    checksum += packet->seqnum;
    checksum += packet->acknum;
    for (int i = 0; i < 20; ++i)
        checksum += packet->payload[i];
    return checksum;
}

/* called from layer 5, passed the data to be sent to other side */
void A_output_abt(struct msg message) {
    if (A_abt.state != WAIT_LAYER5) {
        printf("  A_output: not yet acked. drop the message: %s\n", message.data);
        return;
    }
    printf("  A_output: send packet: %s\n", message.data);
    struct pkt packet;
    packet.seqnum = A_abt.seq;
    memmove(packet.payload, message.data, 20);
    packet.checksum = get_checksum(&packet);
    A_abt.last_packet = packet;
    A_abt.state = WAIT_ACK;
    tolayer3(0, packet);
    starttimer(0, A_abt.estimated_rtt);
}

/* need be completed only for extra credit */
void B_output_abt(struct msg message) {
    printf("  Uni-direcional, apenas igonere.\n funcao: B_output: \n");
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input_abt(struct pkt packet) {
    if (A_abt.state != WAIT_ACK) {
        printf("  A_input: A->B only. drop.\n");
        return;
    }
    if (packet.checksum != get_checksum(&packet)) {
        printf("  A_input: packet corrupted. drop.\n");
        return;
    }

    if (packet.acknum != A_abt.seq) {
        printf("  A_input: not the expected ACK. drop.\n");
        return;
    }
    printf("  A_input: acked.\n");
    stoptimer(0);
    A_abt.seq = 1 - A_abt.seq;
    A_abt.state = WAIT_LAYER5;
}

/* called when A's timer goes off */
void A_timerinterrupt_abt(void) {
    if (A_abt.state != WAIT_ACK) {
        printf("  A_timerinterrupt: not waiting ACK. ignore event.\n");
        return;
    }
    printf("  A_timerinterrupt: resend last packet: %s.\n", A_abt.last_packet.payload);
    tolayer3(0, A_abt.last_packet);
    starttimer(0, A_abt.estimated_rtt);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init_abt(void) {
    A_abt.state = WAIT_LAYER5;
    A_abt.seq = 0;
    A_abt.estimated_rtt = 15;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void send_ack(int AorB, int ack) {
    struct pkt packet;
    packet.acknum = ack;
    packet.checksum = get_checksum(&packet);
    tolayer3(AorB, packet);
}

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input_abt(struct pkt packet) {
    if (packet.checksum != get_checksum(&packet)) {
        printf("  B_input: packet corrupted. send NAK.\n");
        send_ack(1, 1 - B_abt.seq);
        return;
    }
    if (packet.seqnum != B_abt.seq) {
        printf("  B_input: not the expected seq. send NAK.\n");
        send_ack(1, 1 - B_abt.seq);
        return;
    }
    printf("  B_input: recv message: %s\n", packet.payload);
    printf("  B_input: send ACK.\n");
    send_ack(1, B_abt.seq);
    tolayer5(1, packet.payload);
    B_abt.seq = 1 - B_abt.seq;
}

/* called when B's timer goes off */
void B_timerinterrupt_abt(void) {
    printf(" Nada por aqui campeao.\n funcao: B_timerinterrupt.\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init_abt(void) {
    B_abt.seq = 0;
}

/********* GO-BACK-N *********/

#define BUFSIZE 64

struct Sender_gbn {
    int base;
    int nextseq;
    int window_size;
    float estimated_rtt;
    int buffer_next;
    struct pkt packet_buffer[BUFSIZE];
} A_gbn;

struct Receiver_gbn {
    int expect_seq;
    struct pkt packet_to_send;
} B_gbn;

void send_window(void) {
    while (A_gbn.nextseq < A_gbn.buffer_next && A_gbn.nextseq < A_gbn.base + A_gbn.window_size) {
        struct pkt *packet = &A_gbn.packet_buffer[A_gbn.nextseq % BUFSIZE];
        printf("  send_window: send packet (seq=%d): %s\n", packet->seqnum, packet->payload);
        tolayer3(0, *packet);
        if (A_gbn.base == A_gbn.nextseq)
            starttimer(0, A_gbn.estimated_rtt);
        ++A_gbn.nextseq;
    }
}

/* called from layer 5, passed the data to be sent to other side */
void A_output_gbn(  struct msg message){
  if (A_gbn.buffer_next - A_gbn.base >= BUFSIZE) {
        printf("  A_output: buffer full. drop the message: %s\n", message.data);
        return;
    }
    printf("  A_output: bufferred packet (seq=%d): %s\n", A_gbn.buffer_next, message.data);
    struct pkt *packet = &A_gbn.packet_buffer[A_gbn.buffer_next % BUFSIZE];
    packet->seqnum = A_gbn.buffer_next;
    memmove(packet->payload, message.data, 20);
    packet->checksum = get_checksum(packet);
    ++A_gbn.buffer_next;
    send_window();
}

void B_output_gbn(struct msg message)  /* need be completed only for extra credit */{
    printf("  Uni-direcional, apenas igonere.\n funcao: B_output: \n");
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input_gbn(struct pkt packet){
  if (packet.checksum != get_checksum(&packet)) {
        printf("  A_input: packet corrupted. drop.\n");
        return;
    }
    if (packet.acknum < A_gbn.base) {
        printf("  A_input: got NAK (ack=%d). drop.\n", packet.acknum);
        return;
    }
    printf("  A_input: got ACK (ack=%d)\n", packet.acknum);
    A_gbn.base = packet.acknum + 1;
    if (A_gbn.base == A_gbn.nextseq) {
        stoptimer(0);
        printf("  A_input: stop timer\n");
        send_window();
    } else {
        starttimer(0, A_gbn.estimated_rtt);
        printf("  A_input: timer + %f\n", A_gbn.estimated_rtt);
    }
}

/* called when A's timer goes off */
void A_timerinterrupt_gbn(){
   for (int i = A_gbn.base; i < A_gbn.nextseq; ++i) {
        struct pkt *packet = &A_gbn.packet_buffer[i % BUFSIZE];
        printf("  A_timerinterrupt: resend packet (seq=%d): %s\n", packet->seqnum, packet->payload);
        tolayer3(0, *packet);
    }
    starttimer(0, A_gbn.estimated_rtt);
    printf("  A_timerinterrupt: timer + %f\n", A_gbn.estimated_rtt);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init_gbn(){
    A_gbn.base = 1;
    A_gbn.nextseq = 1;
    A_gbn.window_size = 8;
    A_gbn.estimated_rtt = 15;
    A_gbn.buffer_next = 1;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input_gbn(struct pkt packet) {
  if (packet.checksum != get_checksum(&packet)) {
        printf("  B_input: packet corrupted. send NAK (ack=%d)\n", B_gbn.packet_to_send.acknum);
        tolayer3(1, B_gbn.packet_to_send);
        return;
    }
    if (packet.seqnum != B_gbn.expect_seq) {
        printf("  B_input: not the expected seq. send NAK (ack=%d)\n", B_gbn.packet_to_send.acknum);
        tolayer3(1, B_gbn.packet_to_send);
        return;
    }

    printf("  B_input: recv packet (seq=%d): %s\n", packet.seqnum, packet.payload);
    tolayer5(1, packet.payload);

    printf("  B_input: send ACK (ack=%d)\n", B_gbn.expect_seq);
    B_gbn.packet_to_send.acknum = B_gbn.expect_seq;
    B_gbn.packet_to_send.checksum = get_checksum(&B_gbn.packet_to_send);
    tolayer3(1, B_gbn.packet_to_send);

    ++B_gbn.expect_seq;
}

/* called when B's timer goes off */
void B_timerinterrupt_gbn(){
    printf(" Nada por aqui campeao.\n funcao: B_timerinterrupt.\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init_gbn(){
    B_gbn.expect_seq = 1;
    B_gbn.packet_to_send.seqnum = -1;
    B_gbn.packet_to_send.acknum = 0;
    memset(B_gbn.packet_to_send.payload, 0, 20);
    B_gbn.packet_to_send.checksum = get_checksum(&B_gbn.packet_to_send);
}



/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF   0
#define  ON    1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */ 
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */   
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

void init(int argc, char **argv);
void generate_next_arrival(void);
void insertevent(struct event *p);

int main(int argc, char **argv) {
    struct event *eventptr;
    struct msg msg2give;
    struct pkt pkt2give;

    int i, j, tipo;
    char c;
    

    printf("1- Bit alternante\n2- Go-back-N\n\n");
    scanf("%i", &tipo);

    init(argc, argv);

    if(tipo == 1){    
        A_init_abt();
        B_init_abt();

        while (1) {
            eventptr = evlist; /* get next event to simulate */
            if (eventptr == NULL)
                goto terminate;
            evlist = evlist->next; /* remove this event from event list */
            if (evlist != NULL)
                evlist->prev = NULL;
            if (TRACE >= 2) {
                printf("\nEVENT time: %f,", eventptr->evtime);
                printf("  type: %d", eventptr->evtype);
                if (eventptr->evtype == 0)
                    printf(", timerinterrupt  ");
                else if (eventptr->evtype == 1)
                    printf(", fromlayer5 ");
                else
                    printf(", fromlayer3 ");
                printf(" entity: %d\n", eventptr->eventity);
            }
            time = eventptr->evtime; /* update time to next event time */
            if (eventptr->evtype == FROM_LAYER5) {
                if (nsim < nsimmax) {
                    if (nsim + 1 < nsimmax)
                        generate_next_arrival(); /* set up future arrival */
                    /* fill in msg to give with string of same letter */
                    j = nsim % 26;
                    for (i = 0; i < 20; i++)
                        msg2give.data[i] = 97 + j;
                    msg2give.data[19] = 0;
                    if (TRACE > 2) {
                        printf("          MAINLOOP: data given to student: ");
                        for (i = 0; i < 20; i++)
                            printf("%c", msg2give.data[i]);
                        printf("\n");
                    }
                    nsim++;
                    if (eventptr->eventity == A)
                        A_output_abt(msg2give);
                    else
                        B_output_abt(msg2give);
                }
            } else if (eventptr->evtype == FROM_LAYER3) {
                pkt2give.seqnum = eventptr->pktptr->seqnum;
                pkt2give.acknum = eventptr->pktptr->acknum;
                pkt2give.checksum = eventptr->pktptr->checksum;
                for (i = 0; i < 20; i++)
                    pkt2give.payload[i] = eventptr->pktptr->payload[i];
                if (eventptr->eventity == A) /* deliver packet by calling */
                    A_input_abt(pkt2give); /* appropriate entity */
                else
                    B_input_abt(pkt2give);
                free(eventptr->pktptr); /* free the memory for packet */
            } else if (eventptr->evtype == TIMER_INTERRUPT) {
                if (eventptr->eventity == A)
                    A_timerinterrupt_abt();
                else
                    B_timerinterrupt_abt();
            } else {
                printf("INTERNAL PANIC: unknown event type \n");
            }
            free(eventptr);
        }
    }else if(tipo == 2){
        A_init_gbn();
        B_init_gbn();

        while (1) {
            eventptr = evlist; /* get next event to simulate */
            if (eventptr == NULL)
                goto terminate;
            evlist = evlist->next; /* remove this event from event list */
            if (evlist != NULL)
                evlist->prev = NULL;
            if (TRACE >= 2) {
                printf("\nEVENT time: %f,", eventptr->evtime);
                printf("  type: %d", eventptr->evtype);
                if (eventptr->evtype == 0)
                    printf(", timerinterrupt  ");
                else if (eventptr->evtype == 1)
                    printf(", fromlayer5 ");
                else
                    printf(", fromlayer3 ");
                printf(" entity: %d\n", eventptr->eventity);
            }
            time = eventptr->evtime; /* update time to next event time */
            if (eventptr->evtype == FROM_LAYER5) {
                if (nsim < nsimmax) {
                    if (nsim + 1 < nsimmax)
                        generate_next_arrival(); /* set up future arrival */
                    /* fill in msg to give with string of same letter */
                    j = nsim % 26;
                    for (i = 0; i < 20; i++)
                        msg2give.data[i] = 97 + j;
                    msg2give.data[19] = 0;
                    if (TRACE > 2) {
                        printf("          MAINLOOP: data given to student: ");
                        for (i = 0; i < 20; i++)
                            printf("%c", msg2give.data[i]);
                        printf("\n");
                    }
                    nsim++;
                    if (eventptr->eventity == A)
                        A_output_gbn(msg2give);
                    else
                        B_output_gbn(msg2give);
                }
            } else if (eventptr->evtype == FROM_LAYER3) {
                pkt2give.seqnum = eventptr->pktptr->seqnum;
                pkt2give.acknum = eventptr->pktptr->acknum;
                pkt2give.checksum = eventptr->pktptr->checksum;
                for (i = 0; i < 20; i++)
                    pkt2give.payload[i] = eventptr->pktptr->payload[i];
                if (eventptr->eventity == A) /* deliver packet by calling */
                    A_input_gbn(pkt2give); /* appropriate entity */
                else
                    B_input_gbn(pkt2give);
                free(eventptr->pktptr); /* free the memory for packet */
            } else if (eventptr->evtype == TIMER_INTERRUPT) {
                if (eventptr->eventity == A)
                    A_timerinterrupt_gbn();
                else
                    B_timerinterrupt_gbn();
            } else {
                printf("INTERNAL PANIC: unknown event type \n");
            }
            free(eventptr);
        }
        
    }
    terminate:
        printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
        printf("\n\n-------------------------------END-------------------------------");
}

void init(int argc, char **argv) /* initialize the simulator */{
    int i;
    float sum, avg;
    float jimsrand();

    if (argc != 6) {
        printf("usage: %s  num_sim  prob_loss  prob_corrupt  interval  debug_level\n", argv[0]);
        exit(1);
    }

    nsimmax = atoi(argv[1]);
    lossprob = atof(argv[2]);
    corruptprob = atof(argv[3]);
    lambda = atof(argv[4]);
    TRACE = atoi(argv[5]);
    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
    printf("the number of messages to simulate: %d\n", nsimmax);
    printf("packet loss probability: %f\n", lossprob);
    printf("packet corruption probability: %f\n", corruptprob);
    printf("average time between messages from sender's layer5: %f\n", lambda);
    printf("TRACE: %d\n", TRACE);


    srand(9999); /* init random number generator */
    sum = 0.0;   /* test random number generator for students */
    for (i = 0; i < 1000; i++)
        sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
    avg = sum / 1000.0;
    if (avg < 0.25 || avg > 0.75) {
        printf("It is likely that random number generation on your machine\n");
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(1);
    }

    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;

    time = 0.0;              /* initialize time to 0.0 */
    generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand() {
    double mmm = RAND_MAX;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
    float x;                   /* individual students may need to change mmm */ 
    x = rand()/mmm;            /* x should be uniform in [0,1] */
    return(x);
}  

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
void generate_next_arrival(void) {
    double x,log(),ceil();
    struct event *evptr;
    float ttime;
    int tempint;

    if (TRACE>2)
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
  
    x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                              /* having mean of lambda        */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime =  time + x;
    evptr->evtype =  FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand()>0.5) )
        evptr->eventity = B;
      else
        evptr->eventity = A;
    insertevent(evptr);
} 


void insertevent(struct event *p) {
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

void printevlist(void) {
    struct event *q;
    int i;
    printf("--------------\nEvent List Follows:\n");
    for(q = evlist; q!=NULL; q=q->next) {
      printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
      }
    printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB /* A or B is trying to stop timer */){
  struct event *q,*qold;

  if (TRACE > 2)
        printf("          STOP TIMER: stopping timer at %f\n", time);
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
            /* remove this event */
            if (q->next == NULL && q->prev == NULL)
                evlist = NULL;          /* remove first and only event on list */
            else if (q->next == NULL) /* end of list - there is one in front */
                q->prev->next = NULL;
            else if (q == evlist) { /* front of list - there must be event after */
                q->next->prev = NULL;
                evlist = q->next;
            } else { /* middle of list */
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(int AorB /* A or B is trying to stop timer */, float increment) {

struct event *q;
struct event *evptr;

if (TRACE > 2)
        printf("          START TIMER: starting timer at %f\n", time);
    /* be nice: check to see if timer is already started, if so, then  warn */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }

    /* create future event for when timer goes off */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
} 



/************************** TOLAYER3 ***************/
void tolayer3(int AorB /* A or B is trying to stop timer */, struct pkt packet) {
    struct pkt *mypktptr;
    struct event *evptr, *q;
    float lastime, x;
    int i;

    ntolayer3++;

    /* simulate losses: */
    if (jimsrand() < lossprob) {
        nlost++;
        if (TRACE > 0)
            printf("          TOLAYER3: packet being lost\n");
        return;
    }

    /* make a copy of the packet student just gave me since he/she may decide */
    /* to do something with the packet after we return back to him/her */
    mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i = 0; i < 20; i++)
        mypktptr->payload[i] = packet.payload[i];
    if (TRACE > 2) {
        printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
                     mypktptr->acknum, mypktptr->checksum);
        for (i = 0; i < 20; i++)
            printf("%c", mypktptr->payload[i]);
        printf("\n");
    }

    /* create future event for arrival of packet at the other side */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
    evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
    evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */
                                      /* finally, compute the arrival time of packet at the other end.
                                         medium can not reorder, so make sure packet arrives between 1 and 10
                                         time units after the latest arrival time of packets
                                         currently in the medium on their way to the destination */
    lastime = time;
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
            lastime = q->evtime;
    evptr->evtime = lastime + 1 + 9 * jimsrand();

    /* simulate corruption: */
    if (jimsrand() < corruptprob) {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
            mypktptr->payload[0] = 'Z'; /* corrupt payload */
        else if (x < .875)
            mypktptr->seqnum = 999999;
        else
            mypktptr->acknum = 999999;
        if (TRACE > 0)
            printf("          TOLAYER3: packet being corrupted\n");
    }

    if (TRACE > 2)
        printf("          TOLAYER3: scheduling arrival on other side\n");
    insertevent(evptr);
}

void tolayer5(int AorB, char datasent[20]) {
    int i;
    if (TRACE > 2) {
        printf("          TOLAYER5: data received: ");
        for (i = 0; i < 20; i++)
            printf("%c", datasent[i]);
        printf("\n");
    }
}