/* from SLEPc SVD example 14 */
static const char help[] = "Computes the SVD of a matrix (argument -input) and writes the result to two binary files.\n"
  "The first file (argument -write_vals) stores the eigenvalues, and the second (argument -write_vecs) stores the singular vectors\n"
  "The input must be in PETSc binary format, and the outputs will also be written in that format.\n\n";

#include <slepcsvd.h>


PetscErrorCode ReadMatrixFromBinary(MPI_Comm comm, const char *file, Mat *A)
{
  PetscViewer v;
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = MatCreate(comm,A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(*A);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(comm,file,FILE_MODE_READ,&v);CHKERRQ(ierr);
  ierr = MatLoad(*A,v);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

int main(int argc, char **argv)
{
  Mat            A;
  SVD            svd;
  SVDType        type;
  PetscReal      tol;
  PetscInt       N,niter,maxiter,rows,cols,nsv;
  char           matfile[PETSC_MAX_PATH_LEN], evalfile[PETSC_MAX_PATH_LEN], evecfile[PETSC_MAX_PATH_LEN];
  PetscBool      flg,print;
  PetscViewer    viewer;
  PetscErrorCode ierr;

  ierr = SlepcInitialize(&argc,&argv,NULL,help);if(ierr) return ierr;
  fprintf(stderr,"Initialized.\n");
  PetscOptionsGetString(NULL,NULL,"-input",matfile,sizeof(matfile),&flg);
  if (!flg) {
    SETERRQ(PETSC_COMM_WORLD,1,"You must provide an input file (-input)");
  }

  PetscOptionsGetString(NULL,NULL,"-write_vals",evalfile,sizeof(evalfile),&flg);
  if (!flg) {
    const char default_evalfile[] = "singular_vals.bin";
    PetscStrncpy(evalfile,default_evalfile,sizeof(default_evalfile));
  }
  PetscOptionsGetString(NULL,NULL,"-write_vecs",evecfile,sizeof(evecfile),&flg);
  if (!flg) {
    const char default_evecfile[] = "singular_vecs.bin";
    PetscStrncpy(evecfile,default_evecfile,sizeof(default_evecfile));
  }
  PetscOptionsHasName(NULL,NULL,"-print",&print);
  PetscPrintf(PETSC_COMM_WORLD,"Reading matrix from file %s\n",matfile);
  ierr = ReadMatrixFromBinary(PETSC_COMM_WORLD,matfile,&A);CHKERRQ(ierr);
  ierr = MatGetSize(A,&rows,&cols);CHKERRQ(ierr);
  PetscPrintf(PETSC_COMM_WORLD,"Read %Dx%D matrix from the file\n",rows,cols);
  nsv = PetscMin(rows,cols);CHKERRQ(ierr);
  ierr = SVDCreate(PETSC_COMM_WORLD,&svd);
  ierr = SVDSetOperator(svd,A);CHKERRQ(ierr);
  ierr = SVDSetDimensions(svd,nsv,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);
  ierr = SVDSetFromOptions(svd);CHKERRQ(ierr);
  
  ierr = SVDSolve(svd);CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,evalfile,FILE_MODE_WRITE,&viewer);
  ierr = SVDValuesView(svd,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,evecfile,FILE_MODE_WRITE,&viewer);
  ierr = SVDVectorsView(svd,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);

  if (print) {
    ierr = SVDGetIterationNumber(svd,&niter);CHKERRQ(ierr);
    ierr = SVDGetType(svd,&type);CHKERRQ(ierr);
    ierr = SVDGetDimensions(svd,&N,NULL,NULL);CHKERRQ(ierr);
    ierr = SVDGetTolerances(svd,&tol,&maxiter);CHKERRQ(ierr);
    PetscPrintf(PETSC_COMM_WORLD,"Computed SVD with solution method %s in %D iterations.\nYou requested %D singular values, with a tolerance of %.5g and a maximum number of iterations of %D.\n",type,niter,N,tol,maxiter);
  }

  ierr = SVDDestroy(&svd);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  SlepcFinalize();
  return ierr;
}
