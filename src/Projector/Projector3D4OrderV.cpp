#include "Projector3D4OrderV.h"

#include <cmath>
#include <iostream>

#include "ElectroMagn.h"
#include "Field3D.h"
#include "Particles.h"
#include "Tools.h"
#include "Patch.h"

using namespace std;


// ---------------------------------------------------------------------------------------------------------------------
// Constructor for Projector3D4OrderV
// ---------------------------------------------------------------------------------------------------------------------
Projector3D4OrderV::Projector3D4OrderV (Params& params, Patch* patch) : Projector3D(params, patch)
{
    dx_inv_   = 1.0/params.cell_length[0];
    dx_ov_dt  = params.cell_length[0] / params.timestep;
    dy_inv_   = 1.0/params.cell_length[1];
    dy_ov_dt  = params.cell_length[1] / params.timestep;
    dz_inv_   = 1.0/params.cell_length[2];
    dz_ov_dt  = params.cell_length[2] / params.timestep;

    one_third = 1.0/3.0;

    //double defined for use in coefficients
    dble_1_ov_384 = 1.0/384.0;
    dble_1_ov_48 = 1.0/48.0;
    dble_1_ov_16 = 1.0/16.0;
    dble_1_ov_12 = 1.0/12.0;
    dble_1_ov_24 = 1.0/24.0;
    dble_19_ov_96 = 19.0/96.0;
    dble_11_ov_24 = 11.0/24.0;
    dble_1_ov_4 = 1.0/4.0;
    dble_1_ov_6 = 1.0/6.0;
    dble_115_ov_192 = 115.0/192.0;
    dble_5_ov_8 = 5.0/8.0;

    i_domain_begin = patch->getCellStartingGlobalIndex(0);
    j_domain_begin = patch->getCellStartingGlobalIndex(1);
    k_domain_begin = patch->getCellStartingGlobalIndex(2);

    nprimy = params.n_space[1] + 1;
    nprimz = params.n_space[2] + 1;
    oversize[0] = params.oversize[0];
    oversize[1] = params.oversize[1];
    oversize[2] = params.oversize[2];
    dq_inv[0] = dx_inv_;
    dq_inv[1] = dy_inv_;
    dq_inv[2] = dz_inv_;


    DEBUG("cell_length "<< params.cell_length[0]);

}


// ---------------------------------------------------------------------------------------------------------------------
// Destructor for Projector3D4OrderV
// ---------------------------------------------------------------------------------------------------------------------
Projector3D4OrderV::~Projector3D4OrderV()
{
}

// ---------------------------------------------------------------------------------------------------------------------
//!  Project current densities & charge : diagFields timstep (not vectorized)
// ---------------------------------------------------------------------------------------------------------------------
//void Projector3D4OrderV::operator() (double* Jx, double* Jy, double* Jz, double* rho, Particles &particles, unsigned int ipart, double invgf, unsigned int bin, std::vector<unsigned int> &b_dim, int* iold, double* deltaold)
void Projector3D4OrderV::operator() (double* Jx, double* Jy, double* Jz, double *rho, Particles &particles, unsigned int istart, unsigned int iend, std::vector<double> *invgf, std::vector<unsigned int> &b_dim, int* iold, double *deltaold, int ipart_ref)
{
    // -------------------------------------
    // Variable declaration & initialization
    // -------------------------------------

    int npart_total = invgf->size();
    int ipo = iold[0];
    int jpo = iold[1];
    int kpo = iold[2];
    int ipom2 = ipo-3;
    int jpom2 = jpo-3;
    int kpom2 = kpo-3;

    int vecSize = 8;
    int bsize = 7*7*7*vecSize;

    double bJx[bsize] __attribute__((aligned(64)));

    double Sx0_buff_vect[48] __attribute__((aligned(64)));
    double Sy0_buff_vect[48] __attribute__((aligned(64)));
    double Sz0_buff_vect[48] __attribute__((aligned(64)));
    double DSx[56] __attribute__((aligned(64)));
    double DSy[56] __attribute__((aligned(64)));
    double DSz[56] __attribute__((aligned(64)));
    double charge_weight[8] __attribute__((aligned(64)));

    // Closest multiple of 8 higher or equal than npart = iend-istart.
    int cell_nparts( (int)iend-(int)istart );
    int nbVec = ( iend-istart+(cell_nparts-1)-((iend-istart-1)&(cell_nparts-1)) ) / vecSize;
    if (nbVec*vecSize != cell_nparts)
        nbVec++;

    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;

    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){
        
        int np_computed(min(cell_nparts-ivect,vecSize));

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        // Jx^(d,p,p)
        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double crx_p = charge_weight[ipart]*dx_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSx[(k-1)*vecSize+ipart];
            }

            double tmp(  crx_p * ( one_third*DSy[ipart]*DSz[ipart] ) );
            for (unsigned int i=1 ; i<7 ; i++) {
                bJx [ ( (i)*49 )*vecSize+ipart] += sum[i]*tmp;
            }

            for (unsigned int k=1 ; k<7 ; k++) {
                double tmp( crx_p * ( 0.5*DSy[ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] + one_third*DSy[ipart]*DSz[k*vecSize+ipart] ) );
                int index( ( k )*vecSize+ipart );
                for (unsigned int i=1 ; i<7 ; i++) {
                    bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
               }
                
            }
            for (unsigned int j=1 ; j<7 ; j++) {
                double tmp( crx_p * ( 0.5*DSz[ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] + one_third*DSy[j*vecSize+ipart]*DSz[ipart] ) );
                int index( ( j*7 )*vecSize+ipart );
                for (unsigned int i=1 ; i<7 ; i++) {
                    bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
                }
            }//i
            for ( int j=1 ; j<7 ; j++) {
                for ( int k=1 ; k<7 ; k++) {
                    double tmp( crx_p * ( Sy0_buff_vect[(j-1)*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] 
                                          + 0.5*DSy[j*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] 
                                          + 0.5*DSz[k*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] 
                                          + one_third*DSy[j*vecSize+ipart]*DSz[k*vecSize+ipart] ) );
                    int index( ( j*7 + k )*vecSize+ipart );
                    for ( int i=1 ; i<7 ; i++) {
                        bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
                   }
                }
            }//i


        } // END ipart (compute coeffs)

    } // END ivect


    int iloc0 = ipom2*b_dim[1]*b_dim[2]+jpom2*b_dim[2]+kpom2;


    int iloc  = iloc0;
    for (unsigned int i=1 ; i<7 ; i++) {
        iloc += b_dim[1]*b_dim[2];
        for (unsigned int j=0 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=0 ; k<7 ; k++) {
                double tmpJx = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJx += bJx [ilocal+ipart];
                }
                Jx[iloc+j*b_dim[2]+k]         += tmpJx;
            }
        }
    }



    // Jy^(p,d,p)
    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;


    cell_nparts = (int)iend-(int)istart;
    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){
        
        int np_computed(min(cell_nparts-ivect,vecSize));

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){
            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double cry_p = charge_weight[ipart]*dy_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSy[(k-1)*vecSize+ipart];
            }

            double tmp( cry_p *one_third*DSz[ipart]*DSx[ipart] );
            for (unsigned int j=1 ; j<7 ; j++) {
                bJx [((j)*7)*vecSize+ipart] += sum[j]*tmp;
            }
            for (unsigned int k=1 ; k<7 ; k++) {
                double tmp( cry_p * (0.5*DSx[0]*Sz0_buff_vect[(k-1)*vecSize+ipart] + one_third*DSz[k*vecSize+ipart]*DSx[ipart]) );
                int index( ( k )*vecSize+ipart );
                for (unsigned int j=1 ; j<7 ; j++) {
                    bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                }
            }
            for (unsigned int i=1 ; i<7 ; i++) {
                double tmp( cry_p * (0.5*DSz[ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart] + one_third*DSz[0]*DSx[i*vecSize+ipart]) );
                int index( ( i*49 )*vecSize+ipart );
                for (unsigned int j=1 ; j<7 ; j++) {
                    bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                }
            }//i
            for (unsigned int i=1 ; i<7 ; i++) {
                for (unsigned int k=1 ; k<7 ; k++) {
                    double tmp( cry_p * (Sz0_buff_vect[(k-1)*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + 0.5*DSz[k*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + 0.5*DSx[i*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart]
                                         + one_third*DSz[k*vecSize+ipart]*DSx[i*vecSize+ipart]) );
                    int index( ( i*49 + k )*vecSize+ipart );
                    for (unsigned int j=1 ; j<7 ; j++) {
                        bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                   }
                }
            }//i
     

        } // END ipart (compute coeffs)
    }


    iloc = iloc0+ipom2*b_dim[2];
    for (unsigned int i=0 ; i<7 ; i++) {
        for (unsigned int j=1 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=0 ; k<7 ; k++) {
                double tmpJy = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJy += bJx [ilocal+ipart];                  
                }
                Jy[iloc+j*b_dim[2]+k] += tmpJy;
            }
        }
        iloc += (b_dim[1]+1)*b_dim[2];
    }

    cell_nparts = (int)iend-(int)istart;
    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;

    
    // Jz^(p,p,d)
    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){

        int np_computed(min(cell_nparts-ivect,vecSize));

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){
            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double crz_p = charge_weight[ipart]*dz_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSz[(k-1)*vecSize+ipart];
            }

            double tmp( crz_p *one_third*DSx[ipart]*DSy[ipart] );
            for (unsigned int k=1 ; k<7 ; k++) {
                bJx[( k )*vecSize+ipart] += sum[k]*tmp;
            }
            for (unsigned int j=1 ; j<7 ; j++) {
                double tmp( crz_p * (0.5*DSx[ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] + one_third*DSx[ipart]*DSy[j*vecSize+ipart]) );
                int index( ( j*7 )*vecSize+ipart );
                for (unsigned int k=1 ; k<7 ; k++) {
                    bJx [ index+k*vecSize ] += sum[k]*tmp;
                }
            }
            for (unsigned int i=1 ; i<7 ; i++) {
                double tmp( crz_p * (0.5*DSy[ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart] + one_third*DSx[i*vecSize+ipart]*DSy[ipart]) );
                int index( ( i*49 )*vecSize+ipart );
                for (unsigned int k=1 ; k<7 ; k++) {
                    bJx [ index+k*vecSize ] += sum[k]*tmp;
                }
            }//i
            for (unsigned int i=1 ; i<7 ; i++) {
                for (unsigned int j=1 ; j<7 ; j++) {
                    double tmp( crz_p * (Sx0_buff_vect[(i-1)*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart]
                                         + 0.5*DSx[i*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart]
                                         + 0.5*DSy[j*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + one_third*DSx[i*vecSize+ipart]*DSy[j*vecSize+ipart]) );
                    int index( ( i*49 + j*7 )*vecSize+ipart );
                    for (unsigned int k=1 ; k<7 ; k++) {
                        bJx [ index+k*vecSize ] += sum[k]*tmp;
                    }
                }
            }//i


        } // END ipart (compute coeffs)

    }

    iloc = iloc0  + jpom2 +ipom2*b_dim[1];
    for (unsigned int i=0 ; i<7 ; i++) {
        for (unsigned int j=0 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=1 ; k<7 ; k++) {
                double tmpJz = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJz +=  bJx[ilocal+ipart];
                }
                Jz [iloc + (j)*(b_dim[2]+1) + k] +=  tmpJz;
            }
        }
        iloc += b_dim[1]*(b_dim[2]+1);
    }


    // rho^(p,p,d)
    cell_nparts = (int)iend-(int)istart;
    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;

    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){

        int np_computed(min(cell_nparts-ivect,vecSize));

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            double delta  = pos - (double)cell;
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                        ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 ;
            DSx [6*vecSize+ipart] =                                        p1 * S4 ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                        ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 ;
            DSy [6*vecSize+ipart] =                                        p1 * S4 ;
            //                            Z                                
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;     
            cell = round(pos);                                             
            cell_shift = cell-kpo-k_domain_begin;                          
            delta  = pos - (double)cell;                                   
            delta2 = delta*delta;                                          
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);                                       
            c0 = (cell_shift ==  0);                                       
            p1 = (cell_shift ==  1);                                       
            DSz [          ipart] = m1 * S0                                        ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 ;
            DSz [6*vecSize+ipart] =                                        p1 * S4 ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){
            for (unsigned int i=0 ; i<7 ; i++) {
                iloc += b_dim[1]*b_dim[2];       
                for (unsigned int j=0 ; j<7 ; j++) {
                    int index( ( i*49 + j*7 )*vecSize+ipart );
                    for (unsigned int k=0 ; k<7 ; k++) {
                        bJx [ index+k*vecSize ] +=  charge_weight[ipart] * DSx[i*vecSize+ipart]*DSy[j*vecSize+ipart]*DSz[k*vecSize+ipart];
                    }
                }
            }//i


        } // END ipart (compute coeffs)

    }
    //int iloc0 = ipom2*b_dim[1]*b_dim[2]+jpom2*b_dim[2]+kpom2;

    iloc = iloc0;
    for (unsigned int i=0 ; i<7 ; i++) {
        for (unsigned int j=0 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=0 ; k<7 ; k++) {
                double tmpRho = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpRho +=  bJx[ilocal+ipart];
                }
                rho [iloc + (j)*(b_dim[2]) + k] +=  tmpRho;
            }
        }
        iloc += b_dim[1]*(b_dim[2]);
    }
    
} // END Project local current densities at dag timestep.


// ---------------------------------------------------------------------------------------------------------------------
//! Project charge : frozen & diagFields timstep (not vectorized)
// ---------------------------------------------------------------------------------------------------------------------
void Projector3D4OrderV::operator() (double* rhoj, Particles &particles, unsigned int ipart, unsigned int type, std::vector<unsigned int> &b_dim)
{
    //Warning : this function is used for frozen species or initialization only and doesn't use the standard scheme.
    //rho type = 0
    //Jx type = 1
    //Jy type = 2
    //Jz type = 3

    // -------------------------------------
    // Variable declaration & initialization
    // -------------------------------------

    int iloc,jloc;
    int ny(nprimy), nz(nprimz), nyz;
    // (x,y,z) components of the current density for the macro-particle
    double charge_weight = (double)(particles.charge(ipart))*particles.weight(ipart);

    if (type > 0) {
        charge_weight *= 1./sqrt(1.0 + particles.momentum(0,ipart)*particles.momentum(0,ipart)
                                     + particles.momentum(1,ipart)*particles.momentum(1,ipart)
                                     + particles.momentum(2,ipart)*particles.momentum(2,ipart));

        if (type == 1){
            charge_weight *= particles.momentum(0,ipart);
        }
        else if (type == 2){
            charge_weight *= particles.momentum(1,ipart);
            ny ++;
        }
        else {
            charge_weight *= particles.momentum(2,ipart); 
            nz ++;
        }
    }
    nyz = ny*nz;

    // variable declaration
    double xpn, ypn, zpn;
    double delta, delta2, delta3, delta4;
    double Sx1[7], Sy1[7], Sz1[7]; // arrays used for the Esirkepov projection method

// Initialize all current-related arrays to zero
    for (unsigned int i=0; i<7; i++) {
        Sx1[i] = 0.;
        Sy1[i] = 0.;
        Sz1[i] = 0.;
    }

    // --------------------------------------------------------
    // Locate particles & Calculate Esirkepov coef. S, DS and W
    // --------------------------------------------------------

    // locate the particle on the primal grid at current time-step & calculate coeff. S1
    xpn = particles.position(0, ipart) * dx_inv_;
    int ip = round(xpn+ 0.5*(type==1));
    delta  = xpn - (double)ip;
    delta2 = delta*delta;
    delta3 = delta2*delta;
    delta4 = delta3*delta;
    Sx1[1] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
    Sx1[2] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sx1[3] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
    Sx1[4] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sx1[5] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;

    ypn = particles.position(1, ipart) * dy_inv_;
    int jp = round(ypn+ 0.5*(type==2));
    delta  = ypn - (double)jp;
    delta2 = delta*delta;
    delta3 = delta2*delta;
    delta4 = delta3*delta;
    Sy1[1] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
    Sy1[2] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sy1[3] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
    Sy1[4] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sy1[5] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;

    zpn = particles.position(2, ipart) * dz_inv_;
    int kp = round(zpn+ 0.5*(type==3));
    delta  = zpn - (double)kp;
    delta2 = delta*delta;
    delta3 = delta2*delta;
    delta4 = delta3*delta;
    Sz1[1] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
    Sz1[2] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sz1[3] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
    Sz1[4] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
    Sz1[5] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;

    // ---------------------------
    // Calculate the total charge
    // ---------------------------
    ip -= i_domain_begin + 3;
    jp -= j_domain_begin + 3;
    kp -= k_domain_begin + 3;

    for (unsigned int i=0 ; i<7 ; i++) {
        iloc = (i+ip)*b_dim[2]*b_dim[1];
        for (unsigned int j=0 ; j<7 ; j++) {
            jloc = (jp+j)*b_dim[2];;
            for (unsigned int k=0 ; k<7 ; k++) {
                rhoj[iloc+jloc+kp+k] += charge_weight * Sx1[i]*Sy1[j]*Sz1[k];
            }
        }
    }//i

} // END Project local current densities (Frozen species)


// ---------------------------------------------------------------------------------------------------------------------
//! Project global current densities : ionization
// ---------------------------------------------------------------------------------------------------------------------
void Projector3D4OrderV::operator() (Field* Jx, Field* Jy, Field* Jz, Particles &particles, int ipart, LocalFields Jion)
{    
} // END Project global current densities (ionize)


// ---------------------------------------------------------------------------------------------------------------------
//! Project current densities : main projector vectorized
// ---------------------------------------------------------------------------------------------------------------------
void Projector3D4OrderV::operator() (double* Jx, double* Jy, double* Jz, Particles &particles, unsigned int istart, unsigned int iend, std::vector<double> *invgf, std::vector<unsigned int> &b_dim, int* iold, double *deltaold, int ipart_ref)
{
    // -------------------------------------
    // Variable declaration & initialization
    // -------------------------------------

    int npart_total = invgf->size();
    int ipo = iold[0];
    int jpo = iold[1];
    int kpo = iold[2];
    int ipom2 = ipo-3;
    int jpom2 = jpo-3;
    int kpom2 = kpo-3;

    int vecSize = 8;
    int bsize = 7*7*7*vecSize;

    double bJx[bsize] __attribute__((aligned(64)));

    double Sx0_buff_vect[48] __attribute__((aligned(64)));
    double Sy0_buff_vect[48] __attribute__((aligned(64)));
    double Sz0_buff_vect[48] __attribute__((aligned(64)));
    double DSx[56] __attribute__((aligned(64)));
    double DSy[56] __attribute__((aligned(64)));
    double DSz[56] __attribute__((aligned(64)));
    double charge_weight[8] __attribute__((aligned(64)));

    // Closest multiple of 8 higher or equal than npart = iend-istart.
    int cell_nparts( (int)iend-(int)istart );
    int nbVec = ( iend-istart+(cell_nparts-1)-((iend-istart-1)&(cell_nparts-1)) ) / vecSize;
    if (nbVec*vecSize != cell_nparts)
        nbVec++;

    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;

    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){
        
        int np_computed(min(cell_nparts-ivect,vecSize));

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        // Jx^(d,p,p)
        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double crx_p = charge_weight[ipart]*dx_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSx[(k-1)*vecSize+ipart];
            }

            double tmp(  crx_p * ( one_third*DSy[ipart]*DSz[ipart] ) );
            for (unsigned int i=1 ; i<7 ; i++) {
                bJx [ ( (i)*49 )*vecSize+ipart] += sum[i]*tmp;
            }

            for (unsigned int k=1 ; k<7 ; k++) {
                double tmp( crx_p * ( 0.5*DSy[ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] + one_third*DSy[ipart]*DSz[k*vecSize+ipart] ) );
                int index( ( k )*vecSize+ipart );
                for (unsigned int i=1 ; i<7 ; i++) {
                    bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
               }
                
            }
            for (unsigned int j=1 ; j<7 ; j++) {
                double tmp( crx_p * ( 0.5*DSz[ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] + one_third*DSy[j*vecSize+ipart]*DSz[ipart] ) );
                int index( ( j*7 )*vecSize+ipart );
                for (unsigned int i=1 ; i<7 ; i++) {
                    bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
                }
            }//i
            for ( int j=1 ; j<7 ; j++) {
                for ( int k=1 ; k<7 ; k++) {
                    double tmp( crx_p * ( Sy0_buff_vect[(j-1)*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] 
                                          + 0.5*DSy[j*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart] 
                                          + 0.5*DSz[k*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] 
                                          + one_third*DSy[j*vecSize+ipart]*DSz[k*vecSize+ipart] ) );
                    int index( ( j*7 + k )*vecSize+ipart );
                    for ( int i=1 ; i<7 ; i++) {
                        bJx [ index+49*(i)*vecSize ] += sum[i]*tmp;
                   }
                }
            }//i


        } // END ipart (compute coeffs)

    } // END ivect


    int iloc0 = ipom2*b_dim[1]*b_dim[2]+jpom2*b_dim[2]+kpom2;


    int iloc  = iloc0;
    for (unsigned int i=1 ; i<7 ; i++) {
        iloc += b_dim[1]*b_dim[2];
        for (unsigned int j=0 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=0 ; k<7 ; k++) {
                double tmpJx = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJx += bJx [ilocal+ipart];
                }
                Jx[iloc+j*b_dim[2]+k]         += tmpJx;
            }
        }
    }



        // Jy^(p,d,p)

    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;


    cell_nparts = (int)iend-(int)istart;
    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){
        
        int np_computed(min(cell_nparts-ivect,vecSize));

        //#pragma omp simd
        //for (unsigned int i=0; i<200; i++)
        //    bJx[i] = 0.;

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){
            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double cry_p = charge_weight[ipart]*dy_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSy[(k-1)*vecSize+ipart];
            }

            double tmp( cry_p *one_third*DSz[ipart]*DSx[ipart] );
            for (unsigned int j=1 ; j<7 ; j++) {
                bJx [((j)*7)*vecSize+ipart] += sum[j]*tmp;
            }
            for (unsigned int k=1 ; k<7 ; k++) {
                double tmp( cry_p * (0.5*DSx[0]*Sz0_buff_vect[(k-1)*vecSize+ipart] + one_third*DSz[k*vecSize+ipart]*DSx[ipart]) );
                int index( ( k )*vecSize+ipart );
                for (unsigned int j=1 ; j<7 ; j++) {
                    bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                }
            }
            for (unsigned int i=1 ; i<7 ; i++) {
                double tmp( cry_p * (0.5*DSz[ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart] + one_third*DSz[0]*DSx[i*vecSize+ipart]) );
                int index( ( i*49 )*vecSize+ipart );
                for (unsigned int j=1 ; j<7 ; j++) {
                    bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                }
            }//i
            for (unsigned int i=1 ; i<7 ; i++) {
                for (unsigned int k=1 ; k<7 ; k++) {
                    double tmp( cry_p * (Sz0_buff_vect[(k-1)*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + 0.5*DSz[k*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + 0.5*DSx[i*vecSize+ipart]*Sz0_buff_vect[(k-1)*vecSize+ipart]
                                         + one_third*DSz[k*vecSize+ipart]*DSx[i*vecSize+ipart]) );
                    int index( ( i*49 + k )*vecSize+ipart );
                    for (unsigned int j=1 ; j<7 ; j++) {
                        bJx [ index+7*j*vecSize ] += sum[j]*tmp;
                   }
                }
            }//i
     

        } // END ipart (compute coeffs)
    }


    iloc = iloc0+ipom2*b_dim[2];
    for (unsigned int i=0 ; i<7 ; i++) {
        for (unsigned int j=1 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=0 ; k<7 ; k++) {
                double tmpJy = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJy += bJx [ilocal+ipart];                  
                }
                Jy[iloc+j*b_dim[2]+k] += tmpJy;
            }
        }
        iloc += (b_dim[1]+1)*b_dim[2];
    }

    cell_nparts = (int)iend-(int)istart;
    #pragma omp simd
    for (unsigned int j=0; j<bsize; j++)
        bJx[j] = 0.;

        // Jz^(p,p,d)
        //for (unsigned int j=0; j<1000; j=j+40)
        //    #pragma omp simd
        //    for (unsigned int i=0; i<8; i++)
        //        bJx[j+i] = 0.;

    for (int ivect=0 ; ivect < cell_nparts; ivect += vecSize ){

        int np_computed(min(cell_nparts-ivect,vecSize));

        //#pragma omp simd
        //for (unsigned int i=0; i<200; i++)
        //    bJx[i] = 0.;

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){

            // locate the particle on the primal grid at former time-step & calculate coeff. S0
            //                            X                                 //
            double delta = deltaold[ivect+ipart-ipart_ref+istart];
            double delta2 = delta*delta;
            double delta3 = delta2*delta;
            double delta4 = delta3*delta;

            Sx0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sx0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sx0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sx0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Y                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sy0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sy0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sy0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sy0_buff_vect[5*vecSize+ipart] = 0.;

            //                            Z                                 //
            delta = deltaold[ivect+ipart-ipart_ref+istart+2*npart_total];
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;

            Sz0_buff_vect[          ipart] = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[  vecSize+ipart] = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[2*vecSize+ipart] = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            Sz0_buff_vect[3*vecSize+ipart] = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            Sz0_buff_vect[4*vecSize+ipart] = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            Sz0_buff_vect[5*vecSize+ipart] = 0.;


            // locate the particle on the primal grid at current time-step & calculate coeff. S1
            //                            X                                 //
            double pos = particles.position(0, ivect+ipart+istart) * dx_inv_;
            int cell = round(pos);
            int cell_shift = cell-ipo-i_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            double S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            double S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            double S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            double m1 = (cell_shift == -1);
            double c0 = (cell_shift ==  0);
            double p1 = (cell_shift ==  1);
            DSx [          ipart] = m1 * S0                                                                         ;
            DSx [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sx0_buff_vect[          ipart] ;
            DSx [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sx0_buff_vect[  vecSize+ipart] ;
            DSx [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sx0_buff_vect[2*vecSize+ipart] ;
            DSx [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sx0_buff_vect[3*vecSize+ipart] ;
            DSx [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sx0_buff_vect[4*vecSize+ipart] ;
            DSx [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Y                                 //
            pos = particles.position(1, ivect+ipart+istart) * dy_inv_;
            cell = round(pos);
            cell_shift = cell-jpo-j_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSy [          ipart] = m1 * S0                                                                         ;
            DSy [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sy0_buff_vect[          ipart] ;
            DSy [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sy0_buff_vect[  vecSize+ipart] ;
            DSy [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sy0_buff_vect[2*vecSize+ipart] ;
            DSy [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sy0_buff_vect[3*vecSize+ipart] ;
            DSy [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sy0_buff_vect[4*vecSize+ipart] ;
            DSy [6*vecSize+ipart] =                                        p1 * S4                                  ;
            //                            Z                                 //
            pos = particles.position(2, ivect+ipart+istart) * dz_inv_;
            cell = round(pos);
            cell_shift = cell-kpo-k_domain_begin;
            delta  = pos - (double)cell;
            delta2 = delta*delta;
            delta3 = delta2*delta;
            delta4 = delta3*delta;
            S0 = dble_1_ov_384   - dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 - dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            S1 = dble_19_ov_96   - dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 + dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S2 = dble_115_ov_192 - dble_5_ov_8   * delta2 + dble_1_ov_4  * delta4;
            S3 = dble_19_ov_96   + dble_11_ov_24 * delta  + dble_1_ov_4  * delta2 - dble_1_ov_6  * delta3 - dble_1_ov_6  * delta4;
            S4 = dble_1_ov_384   + dble_1_ov_48  * delta  + dble_1_ov_16 * delta2 + dble_1_ov_12 * delta3 + dble_1_ov_24 * delta4;
            m1 = (cell_shift == -1);
            c0 = (cell_shift ==  0);
            p1 = (cell_shift ==  1);
            DSz [          ipart] = m1 * S0                                                                         ;
            DSz [  vecSize+ipart] = c0 * S0 + m1 * S1                              - Sz0_buff_vect[          ipart] ;
            DSz [2*vecSize+ipart] = p1 * S0 + c0 * S1 + m1* S2                     - Sz0_buff_vect[  vecSize+ipart] ;
            DSz [3*vecSize+ipart] =           p1 * S1 + c0* S2 + m1 * S3           - Sz0_buff_vect[2*vecSize+ipart] ;
            DSz [4*vecSize+ipart] =                     p1* S2 + c0 * S3 + m1 * S4 - Sz0_buff_vect[3*vecSize+ipart] ;
            DSz [5*vecSize+ipart] =                              p1 * S3 + c0 * S4 - Sz0_buff_vect[4*vecSize+ipart] ;
            DSz [6*vecSize+ipart] =                                        p1 * S4                                  ;

            charge_weight[ipart] = (double)(particles.charge(ivect+istart+ipart))*particles.weight(ivect+istart+ipart);
        }

        #pragma omp simd
        for (int ipart=0 ; ipart<np_computed; ipart++ ){
            //optrpt complains about the following loop but not unrolling it actually seems to give better result.
            double crz_p = charge_weight[ipart]*dz_ov_dt;

            double sum[7];
            sum[0] = 0.;
            for (unsigned int k=1 ; k<7 ; k++) {
                sum[k] = sum[k-1]-DSz[(k-1)*vecSize+ipart];
            }

            double tmp( crz_p *one_third*DSx[ipart]*DSy[ipart] );
            for (unsigned int k=1 ; k<7 ; k++) {
                bJx[( k )*vecSize+ipart] += sum[k]*tmp;
            }
            for (unsigned int j=1 ; j<7 ; j++) {
                double tmp( crz_p * (0.5*DSx[ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart] + one_third*DSx[ipart]*DSy[j*vecSize+ipart]) );
                int index( ( j*7 )*vecSize+ipart );
                for (unsigned int k=1 ; k<7 ; k++) {
                    bJx [ index+k*vecSize ] += sum[k]*tmp;
                }
            }
            for (unsigned int i=1 ; i<7 ; i++) {
                double tmp( crz_p * (0.5*DSy[ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart] + one_third*DSx[i*vecSize+ipart]*DSy[ipart]) );
                int index( ( i*49 )*vecSize+ipart );
                for (unsigned int k=1 ; k<7 ; k++) {
                    bJx [ index+k*vecSize ] += sum[k]*tmp;
                }
            }//i
            for (unsigned int i=1 ; i<7 ; i++) {
                for (unsigned int j=1 ; j<7 ; j++) {
                    double tmp( crz_p * (Sx0_buff_vect[(i-1)*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart]
                                         + 0.5*DSx[i*vecSize+ipart]*Sy0_buff_vect[(j-1)*vecSize+ipart]
                                         + 0.5*DSy[j*vecSize+ipart]*Sx0_buff_vect[(i-1)*vecSize+ipart]
                                         + one_third*DSx[i*vecSize+ipart]*DSy[j*vecSize+ipart]) );
                    int index( ( i*49 + j*7 )*vecSize+ipart );
                    for (unsigned int k=1 ; k<7 ; k++) {
                        bJx [ index+k*vecSize ] += sum[k]*tmp;
                    }
                }
            }//i


        } // END ipart (compute coeffs)

    }

    iloc = iloc0  + jpom2 +ipom2*b_dim[1];
    for (unsigned int i=0 ; i<7 ; i++) {
        for (unsigned int j=0 ; j<7 ; j++) {
            #pragma omp simd
            for (unsigned int k=1 ; k<7 ; k++) {
                double tmpJz = 0.;
                int ilocal = ((i)*49+j*7+k)*vecSize;
                #pragma unroll(8)
                for (int ipart=0 ; ipart<8; ipart++ ){
                    tmpJz +=  bJx[ilocal+ipart];
                }
                Jz [iloc + (j)*(b_dim[2]+1) + k] +=  tmpJz;
            }
        }
        iloc += b_dim[1]*(b_dim[2]+1);
    }
        
    
} // END Project vectorized


// ---------------------------------------------------------------------------------------------------------------------
//! Wrapper for projection
// ---------------------------------------------------------------------------------------------------------------------
void Projector3D4OrderV::operator() (ElectroMagn* EMfields, Particles &particles, SmileiMPI* smpi, int istart, int iend, int ithread, int scell, int clrw, bool diag_flag, bool is_spectral, std::vector<unsigned int> &b_dim, int ispec, int ipart_ref)
{
    if ( istart == iend ) return; //Don't treat empty cells.

    //Independent of cell. Should not be here
    //{   
    std::vector<double> *delta = &(smpi->dynamics_deltaold[ithread]);
    std::vector<double> *invgf = &(smpi->dynamics_invgf[ithread]);
    //}
    int iold[3];


    iold[0] = scell/(nprimy*nprimz)+oversize[0];

    iold[1] = ( (scell%(nprimy*nprimz)) / nprimz )+oversize[1];
    iold[2] = ( (scell%(nprimy*nprimz)) % nprimz )+oversize[2];
   
    
    // If no field diagnostics this timestep, then the projection is done directly on the total arrays
    if (!diag_flag){ 
        if (!is_spectral) {
            double* b_Jx =  &(*EMfields->Jx_ )(0);
            double* b_Jy =  &(*EMfields->Jy_ )(0);
            double* b_Jz =  &(*EMfields->Jz_ )(0);
            (*this)(b_Jx , b_Jy , b_Jz , particles,  istart, iend, invgf, b_dim, iold, &(*delta)[0], ipart_ref);
        }
        else 
            ERROR("TO DO with rho");
        
    // Otherwise, the projection may apply to the species-specific arrays
    } else {
        double* b_Jx  = EMfields->Jx_s [ispec] ? &(*EMfields->Jx_s [ispec])(0) : &(*EMfields->Jx_ )(0) ;
        double* b_Jy  = EMfields->Jy_s [ispec] ? &(*EMfields->Jy_s [ispec])(0) : &(*EMfields->Jy_ )(0) ;
        double* b_Jz  = EMfields->Jz_s [ispec] ? &(*EMfields->Jz_s [ispec])(0) : &(*EMfields->Jz_ )(0) ;
        double* b_rho = EMfields->rho_s[ispec] ? &(*EMfields->rho_s[ispec])(0) : &(*EMfields->rho_)(0) ;
        (*this)(b_Jx , b_Jy , b_Jz , b_rho, particles,  istart, iend, invgf, b_dim, iold, &(*delta)[0], ipart_ref);
    }
}