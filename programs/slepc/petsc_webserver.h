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
   DTYPE_RETRANS=4,
   DTYPE_SUMMARY=5
  } SERVER_MPI_DTYPE;

MPI_Datatype MPI_DTYPES[6];

/* call this at the beginning of the program, but after MPI_Init(), for 
   any program that needs to send any of the XXX_entry types here, as well
   as process_data_summary. The registered types are stored in MPI_DTYPES
   and indexed by the SERVER_MPI_DTYPES enum. */
extern PetscErrorCode register_mpi_types();

typedef struct {
  PetscInt pid, ip, rport, lport;
  char     laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpaccept_entry;

/* creates a PetscBag with PetscBagCreate() to serialize a
   tcpaccept_entry, and stores  a pointer to that entry
   (via PetscBagGetData()) in the first parameter. The second parmeter
   is used to store the PetscBag we create. The third parameter indicates
   how many entries you have created already to ensure that each entry has
   a unique name, even if data like the PID and/or process name are the 
   same. */
extern PetscErrorCode create_tcpaccept_entry_bag(tcpaccept_entry **, PetscBag *, PetscInt);

/* parses a line of the form 
   PID COMM IP RADDR RPORT LADDR LPORT
   which is stored in the second parameter, 
   and the result is written to the first parameter */
extern PetscErrorCode tcpaccept_entry_parse_line(tcpaccept_entry *, char *);

typedef struct {
  PetscInt pid,ip,dport;
  char     saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnect_entry;

/* creates a PetscBag with PetscBagCreate() to serialize a
   tcpconnect_entry, and stores  a pointer to that entry
   (via PetscBagGetData()) in the first parameter. The second parmeter
   is used to store the PetscBag we create. The third parameter indicates
   how many entries you have created already to ensure that each entry has
   a unique name, even if data like the PID and/or process name are the 
   same. */
extern PetscErrorCode create_tcpconnect_entry_bag(tcpconnect_entry **, PetscBag *, PetscInt);

/* parses a line of the form 
   PID COMM IP SADDR DADDR DPORT
   which is stored in the second parameter, 
   and the result is written to the first parameter */
extern PetscErrorCode tcpconnect_entry_parse_line(tcpconnect_entry *, char *);

typedef struct {
  PetscInt  pid,ip,dport;
  PetscReal lat_ms;
  char      saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnlat_entry;

/* creates a PetscBag with PetscBagCreate() to serialize a
   tcpconnlat_entry, and stores  a pointer to that entry
   (via PetscBagGetData()) in the first parameter. The second parmeter
   is used to store the PetscBag we create. The third parameter indicates
   how many entries you have created already to ensure that each entry has
   a unique name, even if data like the PID and/or process name are the 
   same. */
extern PetscErrorCode create_tcpconnlat_entry_bag(tcpconnlat_entry **, PetscBag *, PetscInt);

/* parses a line of the form 
   PID COMM IP SADDR DADDR DPORT LAT(ms)
   which is stored in the second parameter, 
   and the result is written to the first parameter */
extern PetscErrorCode tcpconnlat_entry_parse_line(tcpconnlat_entry *, char *);
#define TIME_LEN 9

typedef struct {
  PetscInt  pid,ip,lport,rport,tx_kb,rx_kb;
  PetscReal ms;
  char      laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN],time[TIME_LEN];
} tcplife_entry;

/* creates a PetscBag with PetscBagCreate() to serialize a
   tcplife_entry, and stores  a pointer to that entry
   (via PetscBagGetData()) in the first parameter. The second parmeter
   is used to store the PetscBag we create. The third parameter indicates
   how many entries you have created already to ensure that each entry has
   a unique name, even if data like the PID and/or process name are the 
   same. */
extern PetscErrorCode create_tcplife_entry_bag(tcplife_entry **, PetscBag *, PetscInt);

/* parses a line of the form 
   PID,COMM,IP,LADDR,LPORT,RADDR,RPORT,TX_KB,RX_KB,MS
   which is stored in the second parameter, 
   and the result is written to the first parameter.
   NOTE: entries of this form can be generated by 
   running `tcplife -s > /path/to/tcplife.data.file`, 
   since they are comma-delimited, unlike the other 
   XXX_entry_parse_line() functions declared in this file
   whose input is space-delimited. */
extern PetscErrorCode tcplife_entry_parse_line(tcplife_entry *, char *);


typedef struct {
  PetscInt pid,ip;
  char     laddr_port[COMM_MAX_LEN],raddr_port[COMM_MAX_LEN],state[COMM_MAX_LEN];/* TODO: find out how long these really should be, cuz this is longer than necessary. not too important though. */
} tcpretrans_entry;

/* creates a PetscBag with PetscBagCreate() to serialize a
   tcpretrans_entry, and stores  a pointer to that entry
   (via PetscBagGetData()) in the first parameter. The second parmeter
   is used to store the PetscBag we create. The third parameter indicates
   how many entries you have created already to ensure that each entry has
   a unique name, even if data like the PID and/or process name are the 
   same. */
extern PetscErrorCode create_tcpretrans_entry_bag(tcpretrans_entry **, PetscBag *, PetscInt);


extern PetscErrorCode tcpretrans_entry_parse_line(tcpretrans_entry *, char *);


typedef struct {
  /* a circular buffer */
  PetscBag   *buf; /* array of items */
  size_t capacity,valid_start,valid_end,num_items;
} entry_buffer;

/* creates a circular buffer that can be used as a message queue, 
   with MPI for message passing. All MPI ranks can read data and store
   it in the buffer. While you can store any type in the buffer that can
   be serialized in a PetscBag, and on any one rank you can in principle
   store multiple types in the same buffer, you CANNOT store multiple types
   in the same buffer and then call buffer_gather() or buffer_gather_summaries()
   on that buffer, as that invokes undefined behavior in MPI_Gather()

   The first parameter is a pointer to the created buffer, and the second 
   parameter is the capacity of the buffer.*/
extern PetscErrorCode buffer_create(entry_buffer *, size_t);

/* frees all memory used by the buffer, calls PetscBagDestroy() on any
   remaining elements */
extern PetscErrorCode buffer_destroy(entry_buffer *);

/* inserts the PetscBag stored in the second parameter in the buffer 
   pointed to by the first. Returns 0 on success, non-zero on failure.
   The only possible failure mode is if the buffer is full, in which case
   the insertion does not happen. */
extern PetscInt       buffer_try_insert(entry_buffer *, PetscBag);

/* returns the current number of items in the buffer pointed to by the first parameter */
extern size_t         buffer_size(entry_buffer *);

extern size_t         buffer_capacity(entry_buffer *);

extern PetscBool      buffer_full(entry_buffer *);

extern PetscBool      buffer_empty(entry_buffer *);

/* gets the object that has spent the most time in the buffer.
   If you call buffer_get_item() multiple times in a row WITHOUT
   calling buffer_pop() in between each call, you will get the same
   PetscBag on each call to buffer_get_item()*/
extern PetscErrorCode buffer_get_item(entry_buffer *, PetscBag *);

/* remove the object that has spent the most time in the buffer,
   and free its memory */
extern PetscErrorCode buffer_pop(entry_buffer *);

/* gathers all buffer summaries to a buffer on root */
extern PetscErrorCode buffer_gather_summaries(entry_buffer *);

/* gather everything in the buffer pointed to by the first parameter to root,
   with all the entries of the type indicated by the second parameter.
   NOTE: if all the entries are not of that type, that will invoke 
   undefined behavior in the MPI_Gather(), and probably cause a crash */
extern PetscErrorCode buffer_gather(entry_buffer *, SERVER_MPI_DTYPE);


typedef struct {
  long long naccept,nconnect,nconnlat,nlife,nretrans,
            tx_kb,rx_kb,nipv4,nipv6;
  PetscReal latms,lifems;
  char      comm[COMM_MAX_LEN];
} process_data;

typedef struct {
  PetscInt  pid,rank;
  long      tx_kb,rx_kb,n_event;
  PetscReal avg_latency,avg_lifetime,fraction_ipv6;
  char      comm[COMM_MAX_LEN];
} process_data_summary;

/* write the summary pointed to by the second parameter to the
   file pointed to by the first */
extern PetscErrorCode summary_view(FILE *, process_data_summary *);

/* create a process_data_summary and store it in the third parameter. The
   first parameter is the PID, and the second is a pointer to the 
   process_data you want to summarize.*/
extern PetscErrorCode process_data_summarize(PetscInt, process_data *, process_data_summary *);

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

static process_data default_pdata = {0,0,0,0,0,0,0,0,0,0.0,0.0,"[unknown]"};

PETSC_HASH_MAP(HMapData,PetscInt,process_data,PetscHashInt,int_equal,default_pdata);

/* sets the values of the process_data to the defaults */
extern PetscErrorCode process_data_initialize(process_data *);

extern PetscReal fraction_ipv6(process_data *);

extern PetscErrorCode create_process_data_bag(process_data **, PetscBag *);

/* the third parameter is the MPI_Comm rank, fourth is the number of the entry*/
extern PetscErrorCode create_process_summary_bag(process_data_summary **, PetscBag *, PetscInt, PetscInt);

typedef struct {
  PetscHMapData ht;
} process_statistics;

extern PetscErrorCode process_statistics_get_summary(process_statistics *, PetscInt, process_data_summary *);

extern PetscErrorCode process_statistics_num_entries(process_statistics *, PetscInt *);

/* second and third parameters are pointers to array of process_data and PetscInt (pid) respectively of length retrieved from process_statistics_num_entries() */
extern PetscErrorCode process_statistics_get_all(process_statistics *, process_data *, PetscInt *);


extern PetscErrorCode process_statistics_init(process_statistics *);

extern PetscErrorCode process_statistics_destroy(process_statistics *);

extern PetscErrorCode process_statistics_add_pdata(process_statistics *, process_data *);

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

extern PetscErrorCode process_statistics_get_pid_data(process_statistics *,
						      PetscInt,
						      process_data *);
						      

typedef struct {
  FILE *file;
  long offset;
} file_wrapper;
/* WARNING: never delete from a file managed by a file_wrapper */


extern long get_file_end_offset(file_wrapper *);

/* returns 0 if no new data; if the return value is non-zero, it is the difference between the new and old end-of-file. However, the file is moved back to the old end, so you can read the new data. */
extern long has_new_data(file_wrapper *);



#endif
