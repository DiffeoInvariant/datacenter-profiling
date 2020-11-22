#ifndef DCPROF_PETSC_WEBSERVER_H
#define DCPROF_PETSC_WEBSERVER_H
#include <petsc.h>



#define IP_ADDR_MAX_LEN 45
#define COMM_MAX_LEN    PETSC_MAX_PATH_LEN

typedef struct {
  PetscInt pid, ip, rport, lport;
  char     laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpaccept_entry;


extern PetscErrorCode create_tcpaccept_entry_bag(tcpaccept_entry **, PetscBag *);
extern PetscErrorCode zero_initialize_tcpaccept_entry(tcpaccept_entry *);
extern PetscErrorCode clear_tcpaccept_entry(tcpaccept_entry *);

typedef struct {
  PetscInt pid,ip,dport;
  char     saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnect_entry;

extern PetscErrorCode create_tcpconnect_entry_bag(tcpconnlat_entry **, PetscBag *);
extern PetscErrorCode zero_initialize_tcpconnect_entry(tcpconnect_entry *);
extern PetscErrorCode clear_tcpconnect_entry(tcpconnect_entry *);

typedef struct {
  PetscInt  pid,ip,dport;
  PetscReal lat_ms;
  char      saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcpconnlat_entry;

extern PetscErrorCode create_tcpconnlat_entry_bag(tcpconnlat_entry **, PetscBag *);
extern PetscErrorCode zero_initialize_tcpconnlat_entry(tcpconnlat_entry *);
extern PetscErrorCode clear_tcpconnlat_entry(tcpconnlat_entry *);

typedef struct {
  PetscInt  pid,lport,rport,tx_kb,rx_kb;
  PetscReal ms;
  char      LADDR[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],comm[COMM_MAX_LEN];
} tcplife_entry;

extern PetscErrorCode create_tcplife_entry_bag(tcplife_entry **, PetscBag *);
extern PetscErrorCode zero_initialize_tcplife_entry(tcplife_entry *);
extern PetscErrorCode clear_tcplife_entry(tcplife_entry *);

typedef struct {
  PetscInt pid,ip;
  char     laddr_port[COMM_MAX_LEN],raddr_port[COMM_MAX_LEN],state[COMM_MAX_LEN];/* TODO: find out how long these really should be, cuz this is longer than necessary. not too important though. */
} tcpretrans_entry;

extern PetscErrorCode register_tcpretrans_entry(tcpretrans_entry **, PetscBag *);
extern PetscErrorCode zero_initialize_tcpretrans_entry(tcpretrans_entry *);
extern PetscErrorCode clear_tcpretrans_entry(tcpretrans_entry *);

#endif
