/*  This file is part of the OpenLB library
 *
 *  Copyright (C) 2020 Alexander Schulz
 *  E-mail contact: info@openlb.net
 *  The most recent release of OpenLB can be downloaded at
 *  <http://www.openlb.net/>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
*/

//This file contains the ZouHe Velocity Boundary
//This is a new version of the Boundary, which only contains free floating functions
#ifndef SET_ZOUHE_VELOCITY_BOUNDARY_HH
#define SET_ZOUHE_VELOCITY_BOUNDARY_HH

#include "setZouHeVelocityBoundary3D.h"

namespace olb {


///Initialising the setZouHeVelocityBoundary function on the superLattice domain
template<typename T,typename DESCRIPTOR, class MixinDynamics>
void setZouHeVelocityBoundary(SuperLattice<T, DESCRIPTOR>& sLattice, T omega, SuperGeometry<T,3>& superGeometry, int material)
{
  setZouHeVelocityBoundary<T,DESCRIPTOR,MixinDynamics>(sLattice, omega, superGeometry.getMaterialIndicator(material));
}
///Initialising the setZouHeVelocityBoundary function on the superLattice domain
template<typename T, typename DESCRIPTOR, class MixinDynamics>
void setZouHeVelocityBoundary(SuperLattice<T, DESCRIPTOR>& sLattice, T omega, FunctorPtr<SuperIndicatorF3D<T>>&& indicator)
{
  OstreamManager clout(std::cout, "setZouHeVelocityBoundary");
  int _overlap = 1;
  bool includeOuterCells = false;
  if (indicator->getSuperGeometry().getOverlap() == 1) {
    includeOuterCells = true;
    clout << "WARNING: overlap == 1, boundary conditions set on overlap despite unknown neighbor materials" << std::endl;
  }

  for (int iC = 0; iC < sLattice.getLoadBalancer().size(); ++iC) {
    setZouHeVelocityBoundary<T,DESCRIPTOR,MixinDynamics>(sLattice.getBlock(iC), omega, indicator->getBlockIndicatorF(iC),includeOuterCells);
  }
  /// Adds needed Cells to the Communicator _commBC in SuperLattice
  addPoints2CommBC(sLattice, std::forward<decltype(indicator)>(indicator), _overlap);
}



/// Set ZouHe velocity boundary for any indicated cells inside the block domain
template<typename T, typename DESCRIPTOR, class MixinDynamics>
void setZouHeVelocityBoundary(BlockLattice<T,DESCRIPTOR>& _block,T omega, BlockIndicatorF3D<T>& indicator, bool includeOuterCells)
{
  using namespace boundaryhelper;
  auto& blockGeometryStructure = indicator.getBlockGeometry();
  const int margin = includeOuterCells ? 0 : 1;
  std::vector<int> discreteNormal(4,0);
  blockGeometryStructure.forSpatialLocations([&](auto iX, auto iY, auto iZ) {
    if (blockGeometryStructure.getNeighborhoodRadius({iX, iY, iZ}) >= margin
        && indicator(iX, iY, iZ)) {
      Dynamics<T,DESCRIPTOR>* dynamics = nullptr;
      PostProcessorGenerator3D<T,DESCRIPTOR>* postProcessor = nullptr;
      discreteNormal = indicator.getBlockGeometry().getStatistics().getType(iX, iY, iZ);
      if (discreteNormal[0] == 0) {
        //setVelocityBoundary
        if (discreteNormal[1] != 0 && discreteNormal[1] == -1) {//set momenta, dynamics and postProcessors on indicated cells
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<0,-1>, 0,-1
          >>();
        }
        else if (discreteNormal[1] != 0 && discreteNormal[1] == 1) {
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<0,1>, 0,1
          >>();
        }
        else if (discreteNormal[2] != 0 && discreteNormal[2] == -1) {
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<1,-1>, 1,-1
          >>();
        }
        else if (discreteNormal[2] != 0 && discreteNormal[2] == 1) {
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<1,1>, 1,1
          >>();
        }
        else if (discreteNormal[3] != 0 && discreteNormal[3] == -1) {
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<2,-1>, 2,-1
          >>();
        }
        else if (discreteNormal[3] != 0 && discreteNormal[3] == 1) {
          dynamics = _block.template getDynamics<ZouHeDynamics<T,DESCRIPTOR, MixinDynamics, 
            momenta::BasicDirichletVelocityBoundaryTuple<2,1>, 2,1
          >>();
        }
      }
      //ExternalVelocityCorner
      else if (discreteNormal[0] == 1) {
            dynamics = _block.template getDynamics<typename MixinDynamics::template exchange_momenta<
              momenta::FixedVelocityBoundaryTuple
            >>();
            _block.addPostProcessor(
              typeid(PostStream), {iX, iY, iZ},
              promisePostProcessorForNormal<T,DESCRIPTOR,OuterVelocityCornerProcessor3D>(
                Vector<int,3>(discreteNormal.data() + 1)));
      }
      //InternalVelocityCorner
      else if (discreteNormal[0] == 2) {//set momenta, dynamics and postProcessors on indicated internal corner cells
            dynamics = _block.getDynamics(PlainMixinDynamicsForNormalMomenta<T,DESCRIPTOR,
              CombinedRLBdynamics,MixinDynamics,momenta::InnerCornerVelocityTuple3D
            >(Vector<int,3>(discreteNormal.data() + 1)));
            postProcessor = nullptr;
      }
      //ExternalVelocityEdge
      else if (discreteNormal[0] == 3) {//set momenta, dynamics and postProcessors on indicated external edge cells
            dynamics = _block.template getDynamics<typename MixinDynamics::template exchange_momenta<
              momenta::FixedVelocityBoundaryTuple
            >>();
            _block.addPostProcessor(
              typeid(PostStream), {iX, iY, iZ},
              promisePostProcessorForNormalSpecial<T,DESCRIPTOR,OuterVelocityEdgeProcessor3D>(
                Vector<int,3>(discreteNormal.data() + 1)));
      }
      //InternalVelocityEdge
      else if (discreteNormal[0] == 4) {//set momenta, dynamics and postProcessors on indicated internal edge cells
            dynamics = _block.getDynamics(PlainMixinDynamicsForNormalSpecialMomenta<T,DESCRIPTOR,
              CombinedRLBdynamics,MixinDynamics,momenta::InnerEdgeVelocityTuple3D
            >::construct(Vector<int,3>(discreteNormal.data() + 1)));
      }
      dynamics->getParameters(_block).template set<descriptors::OMEGA>(omega);
      setBoundary(_block, iX,iY,iZ, dynamics, postProcessor);
    }
  });
}

}//namespace olb
#endif

