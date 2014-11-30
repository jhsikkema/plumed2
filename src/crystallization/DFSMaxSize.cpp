/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2014 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "DFSClustering.h"
#include "core/ActionRegister.h"

//+PLUMEDOC MCOLVARF DFSMAXCLUSTER
/*
Find average properites of atoms in a cluster

\par Examples

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace crystallization {

class DFSMaxCluster : public DFSClustering {
private:
/// The value of beta
  double beta;
///
  bool use_switch;
//
  SwitchingFunction sf;
public:
/// Create manual
  static void registerKeywords( Keywords& keys );
/// Constructor
  DFSMaxCluster(const ActionOptions&);
///
  void doCalculationOnCluster();
};

PLUMED_REGISTER_ACTION(DFSMaxCluster,"DFSMAXCLUSTER")

void DFSMaxCluster::registerKeywords( Keywords& keys ){
  DFSClustering::registerKeywords( keys );
  keys.add("compulsory","BETA","the value of beta to be used in calculating the smooth maximum");
  keys.use("WTOL"); keys.use("USE_ORIENTATION");
  keys.remove("LOWMEM"); keys.use("HIGHMEM");
  keys.add("optional","CSWITCH","use a switching function on the crystallinity parameter");
}

DFSMaxCluster::DFSMaxCluster(const ActionOptions&ao):
Action(ao),
DFSClustering(ao)
{
   // Find out the value of beta
   parse("BETA",beta);
   addValueWithDerivatives(); setNotPeriodic();

   use_switch=false;
   std::string input, errors; parse("CSWITCH",input);
   if( input.length()>0 ){
      use_switch=true; sf.set( input, errors );
      if( errors.length()!=0 ) error("problem reading SWITCH keyword : " + errors );
   }
}

void DFSMaxCluster::doCalculationOnCluster(){
   unsigned size=comm.Get_size(), rank=comm.Get_rank(); 

   std::vector<double> tder( getNumberOfDerivatives() ), fder( 1+getNumberOfDerivatives(), 0.0 );
   MultiValue myvals( getNumberOfQuantities(), getNumberOfDerivatives() );
   std::vector<unsigned> myatoms; std::vector<double> vals( getNumberOfQuantities() );

   fder[0]=0;  // Value and derivatives are accumulated in one array so that there is only one MPI call 
   for(unsigned iclust=0;iclust<getNumberOfClusters();++iclust){
       retrieveAtomsInCluster( iclust+1, myatoms );
       // This deals with filters
       if( myatoms.size()==1 && !isCurrentlyActive(myatoms[0]) ) continue ;

       double vv, df, tval=0; tder.assign( tder.size(), 0.0 );
       for(unsigned j=0;j<myatoms.size();++j){ 
           unsigned i=myatoms[j];
           getVectorForTask( i, false, vals );
           if( use_switch ){
               vv = 1.0 - sf.calculate( vals[1], df );
               tval += vals[0]*vv; df=-df*vals[1];
           } else {
               tval += vals[0]*vals[1]; df=1.; vv=vals[1];
           }
           if( !doNotCalculateDerivatives() ){ 
               getVectorDerivatives( i, false, myvals );
               for(unsigned k=0;k<myvals.getNumberActive();++k){
                   unsigned kat=myvals.getActiveIndex(k);
                   tder[kat]+=vals[0]*df*myvals.getDerivative(1,kat) + vv*myvals.getDerivative(0,kat);
               }
               myvals.clearAll();
           }
       }

       // Accumulate value and derivatives
       fder[0] += exp( tval/beta ); double pref = exp(tval/beta) / beta;
       if( !doNotCalculateDerivatives() ){
           for(unsigned k=0;k<getNumberOfDerivatives();++k) fder[1+k]+=pref*tder[k];
       }
   }

   // And finish the derivatives
   setValue( beta*std::log( fder[0] ) ); Value* myval=getPntrToValue();
   if( !doNotCalculateDerivatives() ){
      double pref = beta/fder[0];
      for(unsigned k=0;k<getNumberOfDerivatives();++k) myval->addDerivative( k, pref*fder[1+k] );
   }
}

}
}