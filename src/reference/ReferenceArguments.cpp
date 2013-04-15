/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

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
#include "ReferenceArguments.h"
#include "ReferenceAtoms.h"
#include "core/Value.h"

namespace PLMD {

ReferenceArguments::ReferenceArguments( const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro),
hasmetric(false)
{
}

void ReferenceArguments::readArgumentsFromPDB( const PDB& pdb ){
  parseVector( "ARG", arg_names );

  reference_args.resize( arg_names.size() );
  metric.resize( arg_names.size(), arg_names.size() );
  for(unsigned i=0;i<arg_names.size();++i) parse( arg_names[i], reference_args[i] );
}

void ReferenceArguments::getArgumentRequests( std::vector<std::string>& argout, bool disable_checks ){
  der_index.resize( arg_names.size() );

  if( argout.size()==0 ){
      for(unsigned i=0;i<arg_names.size();++i){
         argout.push_back( arg_names[i] );
         der_index[i]=i;
      }
  } else {
      if(!disable_checks){
         if( arg_names.size()!=argout.size() ) error("mismatched numbers of arguments in pdb frames");
      }
      bool found;
      for(unsigned i=0;i<arg_names.size();++i){
         found=false;
         if(!disable_checks){
            if( argout[i]!=arg_names[i] ) error("found mismatched arguments in pdb frames");
            der_index[i]=i;
         } else {
            for(unsigned j=0;j<arg_names.size();++j){
              if( argout[j]==arg_names[i] ){ found=true; der_index[i]=j; break; }
            }
            if( !found ){
              der_index[i]=argout.size(); argout.push_back( arg_names[i] );
            }
         }
      }
  }
}

void ReferenceArguments::setMetric( const std::vector<double>& thissigma ){
  hasmetric=true; unsigned k=0;
  for(unsigned i=0;i<reference_args.size();++i){
      for(unsigned j=i;j<reference_args.size();++j){
          metric(i,j)=metric(j,i)=thissigma[k]; k++;
      }
  }
  plumed_massert( thissigma.size()==k, "error metric size does not match what is expected");
}

double ReferenceArguments::calculateArgumentDistance( const std::vector<Value*> vals, const bool& squared ){
  double r=0;
  if( hasmetric ){
      double dp_i, dp_j;
      for(unsigned i=0;i<reference_args.size();++i){
          unsigned ik=der_index[i]; arg_ders[ ik ]=0;
          dp_i=vals[ik]->difference( reference_args[i] );
          for(unsigned j=0;j<reference_args.size();++j){
             unsigned jk=der_index[j];
             if(i==j) dp_j=dp_i;
             else dp_j=vals[jk]->difference( reference_args[j], vals[jk]->get() );

             arg_ders[ ik ]+=metric(i,j)*dp_j;
             r+=dp_i*dp_j*metric(i,j);
          }
      }
  } else {
      double dp_i;
      for(unsigned i=0;i<reference_args.size();++i){
          unsigned ik=der_index[i];
          dp_i=vals[ik]->difference( reference_args[i], vals[ik]->get() );
          r+=dp_i*dp_i; arg_ders[ik]=dp_i;
      }
  }
  if(!squared){ 
    r=sqrt(r); double ir=1.0/r; 
    for(unsigned i=0;i<arg_ders.size();++i) arg_ders[i]*=ir; 
  }
  return r;
}
}