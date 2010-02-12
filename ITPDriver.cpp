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

#include <ITPDriver.h>

#include <sofa/core/ObjectFactory.h>
//#include <sofa/core/objectmodel/XitactEvent.h>
//
////force feedback
#include <sofa/component/controller/ForceFeedback.h>
#include <sofa/component/controller/NullForceFeedback.h>
//
#include <sofa/simulation/common/AnimateBeginEvent.h>
#include <sofa/simulation/common/AnimateEndEvent.h>
//
#include <sofa/simulation/common/Node.h>
#include <cstring>

#include <sofa/component/visualModel/OglModel.h>
#include <sofa/core/objectmodel/KeypressedEvent.h>
#include <sofa/core/objectmodel/KeyreleasedEvent.h>
#include <sofa/core/objectmodel/MouseEvent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


namespace sofa
{

namespace component
{

namespace controller
{

using namespace sofa::defaulttype;
using namespace core::componentmodel::behavior;
using namespace sofa::defaulttype;



extern bool isInitialized;

int initDeviceITP(XiToolData& /*data*/)
{
    if (isInitialized) return 0;
    isInitialized = true;

    const char* vendor = getenv("XITACT_VENDOR");
    if (!vendor || !*vendor)
        vendor = "INRIA_Sophia";
    xiSoftwareVendor(vendor);

    return 0;
}

ITPDriver::ITPDriver()
    : Scale(initData(&Scale, 1.0, "Scale","Default scale applied to the Phantom Coordinates. "))
    , permanent(initData(&permanent, false, "permanent" , "Apply the force feedback permanently"))
    , indexTool(initData(&indexTool, (int)0,"toolIndex", "index of the tool to simulate (if more than 1). Index 0 correspond to first tool."))
{

    this->f_listening.setValue(true);
    data.forceFeedback = new NullForceFeedback();
    noDevice = false;
}

ITPDriver::~ITPDriver()
{
}

void ITPDriver::cleanup()
{
    sout << "ITPDriver::cleanup()" << sendl;

    isInitialized = false;

}

void ITPDriver::setForceFeedback(ForceFeedback* ff)
{
    // the forcefeedback is already set
    if(data.forceFeedback == ff)
    {
        std::cout << "the forcefeedback is already set"  << std::endl;
        return;
    }

    if(data.forceFeedback)
        delete data.forceFeedback;
    data.forceFeedback = ff;
};

void ITPDriver::bwdInit()
{
    simulation::Node *context = dynamic_cast<simulation::Node *>(this->getContext()); // access to current node
    if (dynamic_cast<core::componentmodel::behavior::MechanicalState<Vec1dTypes>*>(context->getMechanicalState()) == NULL)
    {
        this->f_printLog.setValue(true);
        serr<<"ERROR : no MechanicalState<Vec1dTypes> defined... init of ITPDriver faild "<<sendl;
        this->_mstate = NULL;
        return ;
    }
    else
    {
        this->_mstate = dynamic_cast<core::componentmodel::behavior::MechanicalState<Vec1dTypes>*> (context->getMechanicalState());

    }

    //std::cout << "ITPDriver::init()" << std::endl;

    ForceFeedback *ff = context->get<ForceFeedback>();

    if(ff)
    {
        this->setForceFeedback(ff);
        sout << "setForceFeedback(ff) ok" << sendl;
    }


    setDataValue();



    if(initDeviceITP(data)==-1)
    {
        noDevice=true;
        std::cout<<"WARNING NO DEVICE"<<std::endl;
    }
    //std::cerr  << "ITPDriver::init() done" << std::endl;
    xiTrocarAcquire();
    int nbr = this->indexTool.getValue();
    char name[1024];
    char serial[16];
    xiTrocarGetDeviceDescription(nbr, name);
    xiTrocarGetSerialNumber(nbr,serial );

    std::cout << "Tool: " << nbr << std::endl;
    std::cout << "name: " << name << std::endl;
    std::cout << "serial: " << serial << std::endl;

    xiTrocarRelease();
}


void ITPDriver::setDataValue()
{
    /*
    data.scale = Scale.getValue();
    data.forceScale = forceScale.getValue();
    Quat q = orientationBase.getValue();
    q.normalize();
    orientationBase.setValue(q);
    data.world_H_baseITP.set( positionBase.getValue(), q		);
    q=orientationTool.getValue();
    q.normalize();
    data.endITP_H_virtualTool.set(positionTool.getValue(), q);
    data.permanent_feedback = permanent.getValue();
    */
}

void ITPDriver::reset()
{
    this->reinit();
}

void ITPDriver::reinitVisual()
{


}

void ITPDriver::reinit()
{
    this->cleanup();
    this->bwdInit();

    this->reinitVisual();

    this->updateForce();

}


void ITPDriver::updateForce()
{
    std::cout << "updating manually force" << std::endl;

    xiTrocarAcquire();
    XiToolState state;
    xiTrocarQueryStates();
    xiTrocarGetState(indexTool.getValue(), &state);
    XiToolForce manualForce = { 0 };

    for (unsigned int i = 0; i<3; ++i)
        manualForce.tipForce[i] = 10.0;

    manualForce.rollForce = 1.0f;
}

void ITPDriver::handleEvent(core::objectmodel::Event *event)
{
    if (dynamic_cast<sofa::simulation::AnimateBeginEvent *>(event))
    {


        // calcul des angles � partir de la direction propos�e par l'interface...
        // cos(ThetaX) = cx   sin(ThetaX) = sx  cos(ThetaZ) = cz   sin(ThetaZ) = sz .
        // au repos (si cx=1 et cz=1) on a  Axe y
        // on commence par tourner autour de x   puis autour de z
        //   [cz  -sz   0] [1   0   0 ] [0]   [ -sz*cx]
        //   [sz   cz   0]*[0   cx -sx]*[1] = [ cx*cz ]
        //    0    0    1] [0   sx  cx] [0]   [ sx    ]



        xiTrocarAcquire();
        XiToolState state;

        xiTrocarQueryStates();
        xiTrocarGetState(indexTool.getValue(), &state);

        Vector3 dir;

        dir[0] = (double)state.trocarDir[0];
        dir[1] = (double)state.trocarDir[1];
        dir[2] = (double)state.trocarDir[2];



        double thetaX= asin(dir[2]);
        double cx = cos(thetaX);
        double thetaZ=0;

        if (cx > 0.0001)
        {
            thetaZ = -asin(dir[0]/cx);
        }
        else
        {
            this->f_printLog.setValue(true);
            serr<<"WARNING can not found the position thetaZ of the interface"<<sendl;
        }





        if(_mstate)
        {
            //Assign the state of the device to the rest position of the device.

            if(_mstate->getSize()>5)
            {
                (*_mstate->getX0())[0].x() = thetaX;
                (*_mstate->getX0())[1].x() = thetaZ;
                (*_mstate->getX0())[2].x() = state.toolRoll;
                (*_mstate->getX0())[3].x() = state.toolDepth*Scale.getValue();
                (*_mstate->getX0())[4].x() =state.opening;
                (*_mstate->getX0())[5].x() =state.opening;
            }
            else
            {
                this->f_printLog.setValue(true);
                serr<<"PROBLEM WITH MSTATE SIZE: must be >= 6"<<sendl;
            }

        }


        // ITP button event handling
        XiStateFlags stateFlag;
        stateFlag = state.flags - restState.flags;

        if (stateFlag == XI_ToolButtonMain)
            this->mainButtonPushed();
        else if (stateFlag == XI_ToolButtonLeft)
            this->leftButtonPushed();
        else if (stateFlag == XI_ToolButtonRight)
            this->rightButtonPushed();

    }

    /*
    	//std::cout<<"NewEvent detected !!"<<std::endl;


    	if (dynamic_cast<sofa::simulation::AnimateBeginEvent *>(event))
    	{
    		//getData(); // copy data->servoDeviceData to gDeviceData
    		hdScheduleSynchronous(copyDeviceDataCallback, (void *) &data, HD_MIN_SCHEDULER_PRIORITY);
    		if (data.deviceData.ready)
    		{
    			data.deviceData.quat.normalize();
    			//sout << "driver is working ! " << data->servoDeviceData.transform[12+0] << endl;


    			/// COMPUTATION OF THE vituralTool 6D POSITION IN THE World COORDINATES
    			SolidTypes<double>::Transform baseITP_H_endITP(data.deviceData.pos*data.scale, data.deviceData.quat);
    			SolidTypes<double>::Transform world_H_virtualTool = data.world_H_baseITP * baseITP_H_endITP * data.endITP_H_virtualTool;


    			/// TODO : SHOULD INCLUDE VELOCITY !!
    			sofa::core::objectmodel::XitactEvent ITPEvent(data.deviceData.id, world_H_virtualTool.getOrigin(), world_H_virtualTool.getOrientation() , data.deviceData.m_buttonState);

    			this->getContext()->propagateEvent(&ITPEvent);

    			if (moveITPBase)
    			{
    				std::cout<<" new positionBase = "<<positionBase_buf<<std::endl;
    				visu_base->applyTranslation(positionBase_buf[0] - positionBase.getValue()[0],
    											positionBase_buf[1] - positionBase.getValue()[1],
    											positionBase_buf[2] - positionBase.getValue()[2]);
    				positionBase.setValue(positionBase_buf);
    				setDataValue();
    				//this->reinitVisual();
    			}

    		}
    		else
    			std::cout<<"data not ready"<<std::endl;



    	}

    	if (dynamic_cast<core::objectmodel::KeypressedEvent *>(event))
    	{
    		core::objectmodel::KeypressedEvent *kpe = dynamic_cast<core::objectmodel::KeypressedEvent *>(event);
    		if (kpe->getKey()=='Z' ||kpe->getKey()=='z' )
    		{
    			moveITPBase = !moveITPBase;
    			std::cout<<"key z detected "<<std::endl;
    			visu.setValue(moveITPBase);


    			if(moveITPBase)
    			{
    				this->cleanup();
    				positionBase_buf = positionBase.getValue();

    			}
    			else
    			{
    				this->reinit();
    			}
    		}

    		if(kpe->getKey()=='K' || kpe->getKey()=='k')
    		{
    			positionBase_buf.x()=0.0;
    			positionBase_buf.y()=0.5;
    			positionBase_buf.z()=2.6;
    		}

    		if(kpe->getKey()=='L' || kpe->getKey()=='l')
    		{
    			positionBase_buf.x()=-0.15;
    			positionBase_buf.y()=1.5;
    			positionBase_buf.z()=2.6;
    		}

    		if(kpe->getKey()=='M' || kpe->getKey()=='m')
    		{
    			positionBase_buf.x()=0.0;
    			positionBase_buf.y()=2.5;
    			positionBase_buf.z()=2.6;
    		}



    	}

    */
}



Quat ITPDriver::fromGivenDirection( Vector3& dir,  Vector3& local_dir, Quat old_quat)
{
    local_dir.normalize();
    Vector3 old_dir = old_quat.rotate(local_dir);
    dir.normalize();

    if (dot(dir, old_dir)<1.0)
    {
        Vector3 z = cross(old_dir, dir);
        z.normalize();
        double alpha = acos(dot(old_dir, dir));

        Quat dq, Quater_result;

        dq.axisToQuat(z, alpha);

        Quater_result =  old_quat+dq;

        //std::cout<<"debug - verify fromGivenDirection  dir = "<<dir<<"  Quater_result.rotate(local_dir) = "<<Quater_result.rotate(local_dir)<<std::endl;

        return Quater_result;
    }

    return old_quat;
}


void ITPDriver::mainButtonPushed()
{
    std::cout << "ITPDriver::mainButtonPushed() not yet implemented" << std::endl;
}

void ITPDriver::rightButtonPushed()
{
    std::cout << "ITPDriver::rightButtonPushed() not yet implemented" << std::endl;
}

void ITPDriver::leftButtonPushed()
{
    std::cout << "ITPDriver::leftButtonPushed() not yet implemented" << std::endl;
}


int ITPDriverClass = core::RegisterObject("Driver and Controller of ITP Xitact Device")
        .add< ITPDriver >();

SOFA_DECL_CLASS(ITPDriver)


} // namespace controller

} // namespace component

} // namespace sofa