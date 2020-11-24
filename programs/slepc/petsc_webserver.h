#ifndef DCPROF_PETSC_WEBSERVER_H
#define DCPROF_PETSC_WEBSERVER_H
#include <petsc.h>
#include <unistd.h>
#include <petsc/private/hashtable.h>
#include <petsc/private/hashmap.h>

#define IP_ADDR_MAX_LEN 45
#define COMM_MAX_LEN    PETSC_MAX_PATH_LEN

typedef enum {TCPACCEPT,TCPCONNECT,TCPCONNLAT,TCPLIFE,TCPRETRANS} InputType;
static const char *InputTypes[] = {"ACCEPT","CONNECT","CONNLAT","LIFE","RETRANS","TCP",0};

typedef enum 
  {
   DTYPE_ACCEPT=0,
   DTYPE_CONNECT=1,
   DTYPE_CONNLAT=2,
   DTYPE_LIFE=3,
   DTYPE_RETRANS=4
  } SERVER_MPI_DTYPES;

MPI_Datatype MPI_DTYPES[5];

extern PetscErrorCode register_mpi_types();

typedef struct {
  PetscInt pid, ip, rport, lport;
  char     laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpaccept_entry;


extern PetscErrorCode create_tcpaccept_entry_bag(tcpaccept_entry **, PetscBag *, PetscInt);
extern PetscErrorCode tcpaccept_entry_parse_line(tcpaccept_entry *, char *);

typedef struct {
  PetscInt pid,ip,dport;
  char     saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnect_entry;

extern PetscErrorCode create_tcpconnect_entry_bag(tcpconnect_entry **, PetscBag *, PetscInt);

extern PetscErrorCode tcpconnect_entry_parse_line(tcpconnect_entry *, char *);

typedef struct {
  PetscInt  pid,ip,dport;
  PetscReal lat_ms;
  char      saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnlat_entry;

extern PetscErrorCode create_tcpconnlat_entry_bag(tcpconnlat_entry **e, PetscBag *b, PetscInt);

extern PetscErrorCode tcpconnlat_entry_parse_line(tcpconnlat_entry *e, char *b);
#define TIME_LEN 9

typedef struct {
  PetscInt  pid,ip,lport,rport,tx_kb,rx_kb;
  PetscReal ms;
  char      laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN],time[TIME_LEN];
} tcplife_entry;

extern PetscErrorCode create_tcplife_entry_bag(tcplife_entry **, PetscBag *, PetscInt);

extern PetscErrorCode tcplife_entry_parse_line(tcplife_entry *, char *);


typedef struct {
  PetscInt pid,ip;
  char     laddr_port[COMM_MAX_LEN],raddr_port[COMM_MAX_LEN],state[COMM_MAX_LEN];/* TODO: find out how long these really should be, cuz this is longer than necessary. not too important though. */
} tcpretrans_entry;

extern PetscErrorCode create_tcpretrans_entry_bag(tcpretrans_entry **e, PetscBag *b);

extern PetscErrorCode tcpretrans_entry_parse_line(tcpretrans_entry *e, char *b);


typedef struct {
  /* a circular buffer */
  PetscBag   *buf; /* array of items */
  size_t capacity,valid_start,valid_end,num_items;
} entry_buffer;

extern PetscErrorCode buffer_create(entry_buffer *buf, size_t num_items);
extern PetscErrorCode buffer_destroy(entry_buffer *);

extern PetscInt       buffer_try_insert(entry_buffer *, PetscBag);

extern size_t         buffer_size(entry_buffer *);

extern size_t         buffer_capacity(entry_buffer *);

extern PetscBool      buffer_full(entry_buffer *);

extern PetscBool      buffer_empty(entry_buffer *);

extern PetscErrorCode buffer_get_item(entry_buffer *, PetscBag *);

extern PetscErrorCode buffer_pop(entry_buffer *);



typedef struct {
  long long naccept,nconnect,nconnlat,nlife,nretrans,
            tx_kb,rx_kb,nipv4,nipv6;
  PetscReal latms,lifems;
} process_data;

typedef struct {
  PetscInt  pid;
  long long tx_kb,rx_kb,n_event;
  PetscReal avg_latency,avg_lifetime,fraction_ipv6;
} process_data_summary;

#define pid_hash(pid) pid


#define process_data_equal(lhs,rhs) ( lhs.naccept == rhs.naccept && \
				      lhs.nconnect == rhs.nconnect &&	\
				      lhs.nconnlat == rhs.nconnlat && \
				      lhs.nlife == rhs.nlife &&	      \
				      lhs.tx_kb == rhs.tx_kb && \
				      lhs.rx_kb == rhs.rx_kb &&	\
				      lhs.nipv4 == rhs.nipv4 && \
				      lhs.nipv6 == rhs.nipv6 && \
				      lhs.latms == rhs.latms && \
				      lhs.lifems == rhs.lifems)

#define int_equal(lhs,rhs) lhs == rhs

static process_data default_pdata = {0,0,0,0,0,0,0,0,0,0.0,0.0};

PETSC_HASH_MAP(HMapData,PetscInt,process_data,PetscHashInt,int_equal,default_pdata);
extern PetscErrorCode process_data_initialize(process_data *);

extern PetscReal fraction_ipv6(process_data *);

extern PetscErrorCode create_process_data_bag(process_data **, PetscBag *);

typedef struct {
  PetscHMapData ht;
} process_statistics;

extern PetscErrorCode process_statistics_init(process_statistics *);

extern PetscErrorCode process_statistics_destroy(process_statistics *);

extern PetscErrorCode process_statistics_add_accept(process_statistics *,
						    tcpaccept_entry *);

extern PetscErrorCode process_statistics_add_connect(process_statistics *,
						     tcpconnect_entry *);

extern PetscErrorCode process_statistics_add_connlat(process_statistics *,
						     tcpconnlat_entry *);

extern PetscErrorCode process_statistics_add_life(process_statistics *,
						    tcplife_entry *);

extern PetscErrorCode process_statistics_add_retrans(process_statistics *,
						     tcpretrans_entry *);

typedef struct {
  FILE *file;
  long offset;
} file_wrapper;
/* WARNING: never delete from a file managed by a file_wrapper */


extern long get_file_end_offset(file_wrapper *);

/* returns 0 if no new data; if the return value is non-zero, it is the difference between the new and old end-of-file. However, the file is moved back to the old end, so you can read the new data. */
extern long has_new_data(file_wrapper *);

#endif
