#include "petsc_utils.h"

PetscErrorCode ReadMatrixFromBinary(MPI_Comm comm, const char *file, Mat *A)
{
  PetscViewer v;
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = MatCreate(comm,A);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(comm,file,FILE_MODE_READ,&v);CHKERRQ(ierr);
  ierr = MatLoad(*A,v);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
