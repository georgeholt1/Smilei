
#include "VectorPatch.h"

#include <cstdlib>
#include <iostream>
#include <iomanip>

#include "Hilbert_functions.h"
#include "PatchesFactory.h"
#include "Species.h"
#include "Particles.h"
#include "SmileiIOFactory.h"

using namespace std;

VectorPatch::VectorPatch()
{
}

VectorPatch::~VectorPatch()
{
}

void VectorPatch::exchangeParticles(int ispec, PicParams &params, SmileiMPI* smpi)
{
    int useless(0);

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->initExchParticles(smpi, ispec, params, useless, useless);
    }
    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->initCommParticles(smpi, ispec, params, useless, useless);
    }
    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->finalizeCommParticles(smpi, ispec, params, useless, useless);
	(*this)(ipatch)->vecSpecies[ispec]->sort_part();
    }

}

void VectorPatch::sumRhoJ(unsigned int diag_flag )
{

    Jx_.resize(0);
    Jy_.resize(0);
    Jz_.resize(0);
    rho_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	Jx_.push_back( (*this)(ipatch)->EMfields->Jx_ );
	Jy_.push_back( (*this)(ipatch)->EMfields->Jy_ );
	Jz_.push_back( (*this)(ipatch)->EMfields->Jz_ );
	rho_.push_back( (*this)(ipatch)->EMfields->rho_ );
    }

    sum( Jx_ );
    sum( Jy_ );
    sum( Jz_ );
    if(diag_flag) sum( rho_ );

    Jx_.clear();
    Jy_.clear();
    Jz_.clear();
    rho_.clear();

}

void VectorPatch::sumRhoJs( int ispec )
{
    Jx_.resize(0);
    Jy_.resize(0);
    Jz_.resize(0);
    rho_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	Jx_.push_back( (*this)(ipatch)->EMfields->Jx_s[ispec] );
	Jy_.push_back( (*this)(ipatch)->EMfields->Jy_s[ispec] );
	Jz_.push_back( (*this)(ipatch)->EMfields->Jz_s[ispec] );
	rho_.push_back( (*this)(ipatch)->EMfields->rho_s[ispec] );
    }

    sum( Jx_ );
    sum( Jy_ );
    sum( Jz_ );
    sum( rho_ );

    Jx_.clear();
    Jy_.clear();
    Jz_.clear();
    rho_.clear();
    
}

void VectorPatch::exchangeE( )
{
    Ex_.resize(0);
    Ey_.resize(0);
    Ez_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	Ex_.push_back( (*this)(ipatch)->EMfields->Ex_ );
	Ey_.push_back( (*this)(ipatch)->EMfields->Ey_ );
	Ez_.push_back( (*this)(ipatch)->EMfields->Ez_ );
    }

    exchange( Ex_ );
    exchange( Ey_ );
    exchange( Ez_ );

    Ex_.clear();
    Ey_.clear();
    Ez_.clear();
}

void VectorPatch::exchangeB( )
{
    Bx_.resize(0);
    By_.resize(0);
    Bz_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	Bx_.push_back( (*this)(ipatch)->EMfields->Bx_ );
	By_.push_back( (*this)(ipatch)->EMfields->By_ );
	Bz_.push_back( (*this)(ipatch)->EMfields->Bz_ );
    }
    
    exchange1( Bx_ );
    exchange0( By_  );
    exchange ( Bz_ );

    Bx_.clear();
    By_.clear();
    Bz_.clear();

}

void VectorPatch::computeGlobalDiags(int timestep)
{
    
    computeScalarsDiags(timestep);
    //computeGlobalDiags(probes); // HDF5 write done per patch in DiagProbes::*
    //computeGlobalDiags(phases);
}

void VectorPatch::computeScalarsDiags(int timestep)
{
    int scalars_every( (*this)(0)->Diags->scalars.every );
    if (timestep % scalars_every != 0) return;

    int nDiags( (*this)(0)->Diags->scalars.out_list.size() );
    // Initialize scalars iterator on 1st diag
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->Diags->scalars.itDiagScalar =  (*this)(ipatch)->Diags->scalars.out_list.begin();


    for (int idiags = 0 ; idiags<nDiags ; idiags++) {
	string diagName( (*this)(0)->Diags->scalars.itDiagScalar->first );

	if ( ( diagName.find("Min") == std::string::npos ) && ( diagName.find("Max") == std::string::npos ) ) {
	    double sum(0.);
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		sum += (*this)(ipatch)->Diags->scalars.itDiagScalar->second;
		if (ipatch)
		    (*this)(ipatch)->Diags->scalars.itDiagScalar++;
	    }
	    (*this)(0)->Diags->scalars.itDiagScalar->second = sum;
	    (*this)(0)->Diags->scalars.itDiagScalar++;
	}
	else if ( diagName.find("MinCell") != std::string::npos ) {
	    vector<pair<string,double> >::iterator iterVal    = (*this)(0)->Diags->scalars.itDiagScalar-1;
	    vector<pair<string,double> >::iterator iterValRef = (*this)(0)->Diags->scalars.itDiagScalar-1;
	    double min( iterValRef->second );

	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
		if ((*this)(ipatch)->Diags->scalars.itDiagScalar->second < min) {
		    min = (*this)(ipatch)->Diags->scalars.itDiagScalar->second;
		    iterVal = (*this)(ipatch)->Diags->scalars.itDiagScalar-1;
		}
		if (ipatch)
		    (*this)(ipatch)->Diags->scalars.itDiagScalar++;
	    }
	    (*this)(0)->Diags->scalars.itDiagScalar->second = min;
	    iterValRef->second = iterVal->second;

	    (*this)(0)->Diags->scalars.itDiagScalar++;	    
	}
	else if ( diagName.find("MaxCell") != std::string::npos ) {
	    vector<pair<string,double> >::iterator iterVal    = (*this)(0)->Diags->scalars.itDiagScalar-1;
	    vector<pair<string,double> >::iterator iterValRef = (*this)(0)->Diags->scalars.itDiagScalar-1;
	    double max( iterValRef->second );

	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
		if ((*this)(ipatch)->Diags->scalars.itDiagScalar->second > max) {
		    max = (*this)(ipatch)->Diags->scalars.itDiagScalar->second;
		    iterVal = (*this)(ipatch)->Diags->scalars.itDiagScalar-1;
		}
		if (ipatch)
		    (*this)(ipatch)->Diags->scalars.itDiagScalar++;
	    }
	    (*this)(0)->Diags->scalars.itDiagScalar->second = max;
	    iterValRef->second = iterVal->second;

	    (*this)(0)->Diags->scalars.itDiagScalar++;	    
	}

	// Go to next diag
    }

    // After MPI sync
    //(*this)(0)->Diags->scalars.write(timestep);

}

void VectorPatch::initProbesDiags(PicParams& params, DiagParams &diag_params, int timestep)
{
    (*this)(0)->Diags->probes.createFile(diag_params);
    // Start at 0, cause of setFile set probesStart (locate writing point in h5 file)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->probes.setFile( (*this)(0)->Diags->probes.fileId, (*this)(ipatch), params, diag_params );
    }
    //cout << " File created " << endl;
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	//cout << "Data written for " << ipatch << endl;
	(*this)(ipatch)->Diags->probes.writePositionIn(params, diag_params);
	//cout << "End of Data written for " << ipatch << endl;
    }
}

void VectorPatch::finalizeProbesDiags(PicParams& params, DiagParams &diag_params, int timestep)
{
    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->probes.setFile( 0 );
    }

}

void VectorPatch::initDumpFields(PicParams& params, DiagParams &diag_params, int timestep)
{
    (*this)(0)->sio->createFiles(params, diag_params, (*this)(0));
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->sio->setFiles( (*this)(0)->sio->global_file_id_, (*this)(0)->sio->global_file_id_avg );
    }
}

void VectorPatch::finalizeDumpFields(PicParams& params, DiagParams &diag_params, int timestep)
{
    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->sio->setFiles( 0, 0 );
    }

}

void VectorPatch::createPatches(PicParams& params, DiagParams& diag_params, LaserParams& laser_params, SmileiMPI* smpi, SimWindow* simWindow)
{
    unsigned int n_moved(0);
    recv_patches_.resize(0);

    // Set Index of the 1st patch of the vector yet on current MPI rank
    refHindex_ = (*this)(0)->Hindex();

    recv_patch_id_.clear();
    send_patch_id_.clear();
    
    
    // define recv_patches_ parsing patch_count
    // Go to 1st patch to recv (maybe yet on current CPU)
    // istart = Index of the futur 1st patch
    // recv : store real Hindex
    int istart( 0 );
    for (int irk=0 ; irk<smpi->getRank() ; irk++) istart += smpi->patch_count[irk];
    for (int ipatch=0 ; ipatch<smpi->patch_count[smpi->getRank()] ; ipatch++)
	recv_patch_id_.push_back( istart+ipatch );

    // define send_patches_ parsing patch_count
    // send : store local hindex
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	send_patch_id_.push_back( ipatch );
    }


    std::vector<int> tmp(0);
    for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++)
	if ( ( refHindex_+ipatch<recv_patch_id_[0] ) || ( refHindex_+ipatch>recv_patch_id_[recv_patch_id_.size()-1] ) )
	    tmp.push_back( ipatch );

    int nPatches( recv_patch_id_.size()-1 );
    for ( int ipatch=nPatches ; ipatch>=0 ; ipatch--) {
	if ( ( recv_patch_id_[ipatch]>=refHindex_+send_patch_id_[0] ) && ( recv_patch_id_[ipatch]<=refHindex_+send_patch_id_[send_patch_id_.size()-1] ) ) {
	    recv_patch_id_.erase( recv_patch_id_.begin()+ipatch );
	}
    }

    send_patch_id_ = tmp;

    // Store in local vector future patches
    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
	// density profile is initializes as if t = 0 !
	// Species will be cleared when, nbr of particles will be known
	if (simWindow) n_moved = simWindow->getNmoved(); 
	Patch* newPatch = PatchesFactory::create(params, diag_params, laser_params, smpi, recv_patch_id_[ipatch], n_moved );
	recv_patches_.push_back( newPatch );
    }

}

void VectorPatch::setNbrParticlesToExch(SmileiMPI* smpi)
{
    int nSpecies( (*this)(0)->vecSpecies.size() );
    int nDim_Parts( (*this)(0)->vecSpecies[0]->particles->dimension() );

    // Send particles
    for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {

	int newMPIrank(0);
	// locate rank which will own send_patch_id_[ipatch]
	int tmp( smpi->patch_count[newMPIrank] );
	while ( tmp <= send_patch_id_[ipatch]+refHindex_ ) {
	    newMPIrank++;
	    tmp += smpi->patch_count[newMPIrank];
	}

	vector<int> nbrOfPartsSend(nSpecies,0);
	for (int ispec=0 ; ispec<nSpecies ; ispec++) {
	    nbrOfPartsSend[ispec] = (*this)(send_patch_id_[ipatch])->vecSpecies[ispec]->getNbrOfParticles();
	}
#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " send to " << newMPIrank << " with tag " << refHindex_+send_patch_id_[ipatch] << endl;
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part send = " << nbrOfPartsSend[ispec] << endl;
#endif
	smpi->send( nbrOfPartsSend, newMPIrank, refHindex_+send_patch_id_[ipatch] );
    }


    // Recv part
    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {

	vector<int> nbrOfPartsRecv(nSpecies,0);
	int oldMPIrank(0); // Comparing recv_patch_id_[ipatch] to 1st yet on current MPI rank
	if ( recv_patch_id_[ipatch] > refHindex_ )
	    oldMPIrank = smpi->getRank()+1;
	else
	    oldMPIrank = smpi->getRank()-1;

#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " recv from " << oldMPIrank << " with tag " << recv_patch_id_[ipatch] << endl;
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part recv = " << nbrOfPartsRecv[ispec] << endl;
#endif
	smpi->recv( &nbrOfPartsRecv, oldMPIrank, recv_patch_id_[ipatch] );
#ifdef _DEBUGPATCH
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part recv = " << nbrOfPartsRecv[ispec] << endl;
#endif
	for (int ispec=0 ; ispec<nSpecies ; ispec++)
	    recv_patches_[ipatch]->vecSpecies[ispec]->particles->initialize( nbrOfPartsRecv[ispec], nDim_Parts );
    }

    //Synchro, send/recv must be non-blocking !!!
    smpi->barrier();
}


void VectorPatch::exchangePatches(SmileiMPI* smpi)
{
    int nSpecies( (*this)(0)->vecSpecies.size() );

    // Send part
    for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {

	int newMPIrank(0);
	// locate rank which owns send_patch_id_[ipatch]
	int tmp( smpi->patch_count[newMPIrank] );
	while ( tmp <= send_patch_id_[ipatch]+refHindex_ ) {
	    newMPIrank++;
	    tmp += smpi->patch_count[newMPIrank];
	}
#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " send to " << newMPIrank << " with tag " << send_patch_id_[ipatch] << endl;
#endif
	smpi->send( (*this)(send_patch_id_[ipatch]), newMPIrank, refHindex_+send_patch_id_[ipatch] );

    }


    // Recv part
    // recv_patch_id_ must be sorted !
    // Loop / This, check this->hindex is/not recv_patch_id
    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
	int oldMPIrank(0); // Comparing recv_patch_id_[ipatch] to 1st yet on current MPI rank
	if ( recv_patch_id_[ipatch] > refHindex_ )
	    oldMPIrank = smpi->getRank()+1;
	else
	    oldMPIrank = smpi->getRank()-1;
#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " recv from " << oldMPIrank << " with tag " << recv_patch_id_[ipatch] << endl;
#endif
	smpi->recv( recv_patches_[ipatch], oldMPIrank, recv_patch_id_[ipatch] );
    }

    //Synchro, send/recv must be non-blocking !!!

    /*for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {
	delete (*this)(send_patch_id_[ipatch]-refHindex_);
	patches_[ send_patch_id_[ipatch]-refHindex_ ] = NULL;
	patches_.erase( patches_.begin() + send_patch_id_[ipatch] - refHindex_ );
	
    }*/
    int nPatchSend(send_patch_id_.size());
    for (int ipatch=nPatchSend-1 ; ipatch>=0 ; ipatch--) {
	//Ok while at least 1 old patch stay inon current CPU
	(*this)(send_patch_id_[ipatch])->Diags->probes.setFile(0);
	(*this)(send_patch_id_[ipatch])->sio->setFiles(0,0);
	delete (*this)(send_patch_id_[ipatch]);
	patches_[ send_patch_id_[ipatch] ] = NULL;
	patches_.erase( patches_.begin() + send_patch_id_[ipatch] );
	
    }

    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
	if ( recv_patch_id_[ipatch] > refHindex_ )
	    patches_.push_back( recv_patches_[ipatch] );
	else
	    patches_.insert( patches_.begin()+ipatch, recv_patches_[ipatch] );
    }
    recv_patches_.clear();

#ifdef _DEBUGPATCH
    cout << smpi->getRank() << " number of patches " << this->size() << endl;
#endif
    for (int ipatch=0 ; ipatch<patches_.size() ; ipatch++ ) { 
	(*this)(ipatch)->updateMPIenv(smpi);
    }

    definePatchDiagsMaster();

}

void VectorPatch::definePatchDiagsMaster()
{
    int patchIdMaster(0);
    for (patchIdMaster=0 ; patchIdMaster<patches_.size() ; patchIdMaster++ )
	if ( (*this)(patchIdMaster)->Diags->probes.fileId != 0 ) break;

    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	if ((ipatch!=patchIdMaster) && (patchIdMaster!=patches_.size()) ) { // patchIdMaster!=patches_.size() 
		(*this)(ipatch)->Diags->probes.setFile( (*this)(patchIdMaster)->Diags->probes.fileId );
	}
    }

    for (patchIdMaster=0 ; patchIdMaster<patches_.size() ; patchIdMaster++ )
	if ( (*this)(patchIdMaster)->sio->global_file_id_ != 0 ) break;

    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	if ((ipatch!=patchIdMaster) && (patchIdMaster!=patches_.size()) ) { // patchIdMaster!=patches_.size() 
	    (*this)(ipatch)->sio->setFiles( (*this)(patchIdMaster)->sio->global_file_id_, (*this)(patchIdMaster)->sio->global_file_id_avg );
	}
    }

}


void VectorPatch::solvePoisson( PicParams &params, SmileiMPI* smpi )
{
    unsigned int nx_p2_global = (params.n_space_global[0]+1) * (params.n_space_global[1]+1);

    unsigned int iteration_max = 50000;
    double       error_max     = 1.e-14;
    unsigned int iteration=0;

    // Init & Store internal data (phi, r, p, Ap) per patch
    double rnew_dot_rnew_local(0.);
    double rnew_dot_rnew(0.);    
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->EMfields->initPoisson( (*this)(ipatch) );
	rnew_dot_rnew_local += (*this)(ipatch)->EMfields->compute_r();
    }
    MPI_Allreduce(&rnew_dot_rnew_local, &rnew_dot_rnew, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    Ap_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++)
	Ap_.push_back( (*this)(ipatch)->EMfields->Ap_ );

    // compute control parameter
    double ctrl = rnew_dot_rnew / (double)(nx_p2_global);
	
    // ---------------------------------------------------------
    // Starting iterative loop for the conjugate gradient method
    // ---------------------------------------------------------
    if (smpi->isMaster()) DEBUG(1,"Starting iterative loop for CG method");
    while ( (ctrl > error_max) && (iteration<iteration_max) ) {
        
        iteration++;
        if (smpi->isMaster()) DEBUG(5,"iteration " << iteration << " started with control parameter ctrl = " << ctrl*1.e14 << " x 1e-14");

        // scalar product of the residual
        double r_dot_r = rnew_dot_rnew;

	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) 
	    (*this)(ipatch)->EMfields->compute_Ap( (*this)(ipatch) );

	// Exchange Ap_ (intra & extra MPI)
	exchange( Ap_ );

       // scalar product p.Ap
        double p_dot_Ap       = 0.0;
        double p_dot_Ap_local = 0.0;
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    p_dot_Ap_local += (*this)(ipatch)->EMfields->compute_pAp();
	}
        MPI_Allreduce(&p_dot_Ap_local, &p_dot_Ap, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);


        // compute new potential and residual
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    (*this)(ipatch)->EMfields->update_pand_r( r_dot_r, p_dot_Ap );
	}

        // compute new residual norm
        rnew_dot_rnew       = 0.0;
        rnew_dot_rnew_local = 0.0;
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    rnew_dot_rnew_local += (*this)(ipatch)->EMfields->compute_r();
	}
	MPI_Allreduce(&rnew_dot_rnew_local, &rnew_dot_rnew, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        if (smpi->isMaster()) DEBUG(10,"new residual norm: rnew_dot_rnew = " << rnew_dot_rnew);

        // compute new directio
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    (*this)(ipatch)->EMfields->update_p( rnew_dot_rnew, r_dot_r );
	}

        // compute control parameter
        ctrl = rnew_dot_rnew / (double)(nx_p2_global);
        if (smpi->isMaster()) DEBUG(10,"iteration " << iteration << " done, exiting with control parameter ctrl = " << ctrl);

    }//End of the iterative loop
    
    
    // --------------------------------
    // Status of the solver convergence
    // --------------------------------
    if (iteration == iteration_max) {
        if (smpi->isMaster())
            WARNING("Poisson solver did not converge: reached maximum iteration number: " << iteration
                    << ", relative error is ctrl = " << 1.0e14*ctrl << " x 1e-14");
    }
    else {
        if (smpi->isMaster()) 
            MESSAGE(1,"Poisson solver converged at iteration: " << iteration
                    << ", relative error is ctrl = " << 1.0e14*ctrl << " x 1e-14");
    }

    Ap_.clear();

    // ------------------------------------------
    // Compute the electrostatic fields Ex and Ey
    // ------------------------------------------
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->EMfields->initE( (*this)(ipatch) );

    exchangeE();    
    
    // Centering of the electrostatic fields
    // -------------------------------------
    
    double Ex_WestNorth = 0.0;
    double Ey_WestNorth = 0.0;
    double Ex_EastSouth = 0.0;
    double Ey_EastSouth = 0.0;

    int rank_WestNorth(0), rank_EastSouth(0);
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	// West-North corner
	if ( ((*this)(ipatch)->isWestern()) && ((*this)(ipatch)->isNorthern()) ) {
	    Ex_WestNorth = (*this)(ipatch)->EMfields->getEx_WestNorth();
	    Ey_WestNorth = (*this)(ipatch)->EMfields->getEy_WestNorth();
	    rank_WestNorth = (*this)(ipatch)->MPI_neighborhood_[4]; 
	}
    
	// East-South corner
	if ( ((*this)(ipatch)->isEastern() ) && ((*this)(ipatch)->isSouthern()) ) {
	    Ex_EastSouth = (*this)(ipatch)->EMfields->getEx_EastSouth();
	    Ey_EastSouth = (*this)(ipatch)->EMfields->getEy_EastSouth();
	    rank_EastSouth = (*this)(ipatch)->MPI_neighborhood_[4];
	}
    }

    MPI_Bcast(&Ex_WestNorth, 1, MPI_DOUBLE, rank_WestNorth, MPI_COMM_WORLD);
    MPI_Bcast(&Ey_WestNorth, 1, MPI_DOUBLE, rank_WestNorth, MPI_COMM_WORLD);

    MPI_Bcast(&Ex_EastSouth, 1, MPI_DOUBLE, rank_EastSouth, MPI_COMM_WORLD);
    MPI_Bcast(&Ey_EastSouth, 1, MPI_DOUBLE, rank_EastSouth, MPI_COMM_WORLD);
    

    vector<double> E_Add(2,0.);
    E_Add[0] = -0.5*(Ex_WestNorth+Ex_EastSouth);
    E_Add[1] = -0.5*(Ey_WestNorth+Ey_EastSouth);
    
    // Centering electrostatic fields
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->EMfields->centeringE( E_Add );

}


void VectorPatch::exchange( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal
    gsp[1] = 2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[3]){
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[3]-h0])((n_space[0])*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    memcpy( pt2, pt1, ny_*sizeof(double)); 
	    memcpy( pt1+gsp[0]*ny_, pt2+gsp[0]*ny_, ny_*sizeof(double)); 
	} // End if ( MPI_neighborhood_[4] == MPI_neighborhood_[3] ) 

	if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[1]){
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[1]-h0])(n_space[1]);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int i = 0 ; i < nx_*ny_ ; i += ny_){
		pt2[i] = pt1[i] ;
		pt1[i+gsp[1]] = pt2[i+gsp[1]] ;
	    } 
	} // End if ( MPI_neighborhood_[4] == MPI_neighborhood_[1] ) 

    } // End for( ipatch )

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 0 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 0 );


    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 1 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 1 );


}

void VectorPatch::exchange0( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal
    gsp[1] = 2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[3]){
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[3]-h0])((n_space[0])*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    memcpy( pt2, pt1, ny_*sizeof(double)); 
	    memcpy( pt1+gsp[0]*ny_, pt2+gsp[0]*ny_, ny_*sizeof(double)); 
	} // End if ( MPI_neighborhood_[4] == MPI_neighborhood_[3] ) 


    } // End for( ipatch )

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 0 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 0 );


}

void VectorPatch::exchange1( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal
    gsp[1] = 2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[1]){
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[1]-h0])(n_space[1]);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int i = 0 ; i < nx_*ny_ ; i += ny_){
		pt2[i] = pt1[i] ;
		pt1[i+gsp[1]] = pt2[i+gsp[1]] ;
	    } 
	} // End if ( MPI_neighborhood_[4] == MPI_neighborhood_[1] ) 

    } // End for( ipatch )

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 1 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 1 );


}

void VectorPatch::sum( std::vector<Field*> fields )
{
    unsigned int nx_,ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];
    
    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];
    
    nx_ = fields[0]->dims_[0];
    ny_ = fields[0]->dims_[1];
    
    gsp[0] = 1+2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal
    gsp[1] = 1+2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

        if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[3]){
	    //The patch on my left belongs to the same MPI process than I.
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[3]-h0])(n_space[0]*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int i = 0; i < gsp[0]* ny_ ; i++) pt1[i] += pt2[i];
	    memcpy( pt2, pt1, gsp[0]*ny_*sizeof(double)); 
                    
	}

    }
    
    #pragma omp master
    {
	for (int iDim=0;iDim<1;iDim++) {
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		(*this)(ipatch)->initSumField( fields[ipatch], iDim ); // initialize
	    }

	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		(*this)(ipatch)->finalizeSumField( fields[ipatch], iDim ); // finalize (waitall + sum)
	    }
	}
    }

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

        if ((*this)(ipatch)->MPI_neighborhood_[4] == (*this)(ipatch)->MPI_neighborhood_[1]){
	    //The patch below me belongs to the same MPI process than I.
	    pt1 = &(*fields[(*this)(ipatch)->patch_neighborhood_[1]-h0])(n_space[1]);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int j = 0; j < nx_ ; j++){
		for (unsigned int i = 0; i < gsp[1] ; i++) pt1[i] += pt2[i];
		memcpy( pt2, pt1, gsp[1]*sizeof(double)); 
		pt1 += ny_;
		pt2 += ny_;
	    }
	}

    }

    #pragma omp master
    {
	for (int iDim=1;iDim<2;iDim++) {
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		(*this)(ipatch)->initSumField( fields[ipatch], iDim ); // initialize
	    }

	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		(*this)(ipatch)->finalizeSumField( fields[ipatch], iDim ); // finalize (waitall + sum)
	    }
	}
    }

}
