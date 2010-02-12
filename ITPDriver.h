/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, version 1.0 beta 4      *
*                (c) 2006-2009 MGH, INRIA, USTL, UJF, CNRS                    *
*                                                                             *
* This library is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This library is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this library; if not, write to the Free Software Foundation,     *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.          *
*******************************************************************************
*                               SOFA :: Modules                               *
*                                                                             *
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
#ifndef SOFA_COMPONENT_ITPDRIVER_H
#define SOFA_COMPONENT_ITPDRIVER_H

//Sensable include
#include <sofa/helper/LCPcalc.h>
#include <sofa/defaulttype/SolidTypes.h>
#include <sofa/core/componentmodel/behavior/MechanicalState.h>
#include <sofa/defaulttype/VecTypes.h>
#include <sofa/core/componentmodel/behavior/BaseController.h>
#include <sofa/component/visualModel/OglModel.h>
#include <sofa/component/controller/Controller.h>
#include <sofa/defaulttype/Vec3Types.h>
#include <sofa/defaulttype/Quat.h>
#include "XiTrocarInterface.h"


namespace sofa
{
namespace simulation { class Node; }

namespace component
{
namespace visualModel { class OglModel; }

namespace controller
{

class ForceFeedback;


using namespace sofa::defaulttype;
using core::objectmodel::Data;

typedef struct
{
    ForceFeedback* forceFeedback;
    simulation::Node *context;

    double scale;
    double forceScale;
    bool permanent_feedback;

    // API ITP //
    XiToolState hapticState;  // for the haptic loop
    XiToolState simuState;		 // for the simulation loop
    XiToolForce hapticForce;

} XiToolData;

/**
* Omni driver
*/
class ITPDriver : public Controller
{
    //SOFA_CLASS(ITPDriver,sofa::component::controller::Controller);

public:
    Data<double> Scale;
    Data<bool> permanent;
    Data <int> indexTool;


    XiToolData	data;

    ITPDriver();
    virtual ~ITPDriver();

    virtual void bwdInit();
    virtual void reset();
    void reinit();

    void cleanup();
    //virtual void draw();

    void setForceFeedback(ForceFeedback* ff);



    void setDataValue();
    void reinitVisual();

private:
    sofa::core::componentmodel::behavior::MechanicalState<Vec1dTypes> *_mstate;
    void handleEvent(core::objectmodel::Event *);
    sofa::component::visualmodel::OglModel *visu_base, *visu_end;
    bool noDevice;
    Quat fromGivenDirection( Vector3& dir,  Vector3& local_dir, Quat old_quat);

    XiToolState restState;

    void mainButtonPushed();
    void rightButtonPushed();
    void leftButtonPushed();

    void updateForce();




};

} // namespace controller

} // namespace component

} // namespace sofa

#endif // SOFA_COMPONENT_ITPDRIVER_H