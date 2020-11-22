#ifndef DCPROF_PETSC_WEBSERVER_H
#define DCPROF_PETSC_WEBSERVER_H
#include <petsc.h>



#define IP_ADDR_MAX_LEN 45


typedef struct {
  PetscInt pid, ip, rport, lport;
  char     laddr[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],*comm;
} tcpaccept_entry;


extern PetscErrorCode register_tcpaccept_entry();
extern PetscErrorCode zero_initialize_tcpaccept_entry(tcpaccept_entry *);
extern PetscErrorCode clear_tcpaccept_entry(tcpaccept_entry *);

typedef struct {
  PetscInt pid,ip,dport;
  char     saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],*comm;
} tcpconnect_entry;

extern PetscErrorCode register_tcpconnect_entry();
extern PetscErrorCode zero_initialize_tcpconnect_entry(tcpconnect_entry *);
extern PetscErrorCode clear_tcpconnect_entry(tcpconnect_entry *);

typedef struct {
  PetscInt  pid,ip,dport;
  PetscReal lat_ms;
  char      saddr[IP_ADDR_MAX_LEN],daddr[IP_ADDR_MAX_LEN],*comm;
} tcpconnlat_entry;

extern PetscErrorCode register_tcpconnlat_entry();
extern PetscErrorCode zero_initialize_tcpconnlat_entry(tcpconnlat_entry *);
extern PetscErrorCode clear_tcpconnlat_entry(tcpconnlat_entry *);

typedef struct {
  PetscInt  pid,lport,rport,tx_kb,rx_kb;
  PetscReal ms;
  char      LADDR[IP_ADDR_MAX_LEN],raddr[IP_ADDR_MAX_LEN],*comm;
} tcplife_entry;

extern PetscErrorCode register_tcplife_entry();
extern PetscErrorCode zero_initialize_tcplife_entry(tcplife_entry *);
extern PetscErrorCode clear_tcplife_entry(tcplife_entry *);

typedef struct {
  PetscInt pid,ip;
  char     *laddr_port,*raddr_port,*state;
} tcpretrans_entry;

extern PetscErrorCode register_tcpretrans_entry();
extern PetscErrorCode zero_initialize_tcpretrans_entry(tcpretrans_entry *);
extern PetscErrorCode clear_tcpretrans_entry(tcpretrans_entry *);

#endif
