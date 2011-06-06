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

#include <IHPDriver.h>

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
//sensable namespace
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


#include <sofa/helper/system/thread/CTime.h>


namespace sofa
{

namespace component
{

namespace controller
{

using namespace sofa::defaulttype;
using namespace core::behavior;
using namespace sofa::defaulttype;




SOFA_XITACTPLUGIN_API void UpdateForceFeedBack(void* toolData)
{

    bool display =false;
    static unsigned int ffbCounter=0;/////////////////////////////////////////////////////////
    ffbCounter++;
    if((ffbCounter%500) == 0)
    {
        std::cout<<"Hep UpdateForceFeedBack called :"<<ffbCounter<<std::endl;///////////////////////////////////////////////////////
        display = true;
    }


    XiToolDataIHP* myData = static_cast<XiToolDataIHP*>(toolData);

    // Compute actual tool state:
    xiTrocarAcquire();
    XiToolState state;
    xiTrocarQueryStates();
    xiTrocarGetState(myData->indexTool, &state);


    Vector3 dir;
    dir[0] = (double)state.trocarDir[0];
    dir[1] = (double)state.trocarDir[1];
    dir[2] = (double)state.trocarDir[2];
    //std::cout<<"dir ="<<dir<<std::endl;


    double toolDpth = state.toolDepth;
    double thetaX= asin(dir[2]);
    double cx = cos(thetaX);
    double thetaZ=0;

    if (cx > 0.0001)
    {
        thetaZ = -asin(dir[0]/cx);
    }
    else
    {
        //this->f_printLog.setValue(true);
        std::cerr<<"WARNING can not found the position thetaZ of the interface"<<std::endl;
    }

    /*if (display)
    	std::cout<<" pos found: toolDpth = "<<toolDpth<<"  thetaX ="<<thetaX<<"  thetaZ= "<<thetaZ<<std::endl;
    */

    // Call LCPForceFeedBack
    sofa::defaulttype::Vec1dTypes::VecCoord currentState; //sofa::helper::vector<sofa::defaulttype::Vector1 > currentState;
    sofa::defaulttype::Vec1dTypes::VecDeriv ForceBack; //sofa::helper::vector<sofa::defaulttype::Vector1 > ForceBack;
    sofa::defaulttype::Vec1dTypes::VecDeriv velocity;




    /* Here, in older version, currentState is declared as an vector<Vec1d> which
     * is not compatible in the use of LCPForceFeedback::computeForce(const  VecCoord& state,  VecDeriv& forces) below
     * In the other way, if currentState declare like a Vec1dTypes::VecCoord for compatibility in function computeForce,
     * it will not have a size of 6 double
     * the interrogation point is : is it resonable to declare all LCPForceFeedback in the template Rigid3dTypes instead of Vec1d */
    ForceBack.clear(); velocity.clear();
    currentState.resize(6); ForceBack.resize(6); velocity.resize(6);

    currentState[0][0] = thetaX;
    currentState[1][0] = thetaZ;
    currentState[2][0] = state.toolRoll;
    currentState[3][0] = toolDpth * myData->scale;
    currentState[4][0] = state.opening;
    currentState[5][0] = state.opening;


    if (myData->lcp_true_vs_vm_false)
        myData->lcp_forceFeedback->computeForce(currentState, ForceBack);//Error here
#ifdef SOFA_DEV
    else
        myData->vm_forceFeedback->computeForce(currentState, velocity, ForceBack);
#endif



    double forceX, forceZ; //Y?
    Vector3 tipForce;

    Vector3 z;
    z[0] = 0;
    z[1] = -sin(thetaX);
    z[2] = cx;
    Vector3 x;
    x[0] = 1; x[1] = 0; x[2] = 0;
    double Mx = ForceBack[0][0];
    double Mz = ForceBack[1][0];

    Vector3 OP;

    if (toolDpth > 0.0)
    {
        OP = dir * toolDpth;
        forceX = Mz/dot( cross(z,OP), x);
        forceZ = Mx/dot( cross(x,OP), z);
    }

    tipForce = dir*ForceBack[3][0] + x * forceX + z *forceZ;


    XiToolForce_ ff;
    ff.tipForce[0] = (float)(tipForce[0] * myData->forceScale);
    ff.tipForce[1] = (float)(tipForce[1] * myData->forceScale);
    ff.tipForce[2] = (float)(tipForce[2] * myData->forceScale);



    if ( (abs(ff.tipForce[0]) > FFthresholdX) || (abs(ff.tipForce[1]) > FFthresholdY) || (abs(ff.tipForce[2]) > FFthresholdZ) )
    {
        std::cout << "Error: Force FeedBack has reached a safety threshold! See header file IHPDriver.h." << std::endl;
        std::cout << "F_X: " << ff.tipForce[0] << "F_Y: " << ff.tipForce[1] << "F_Z: " << ff.tipForce[2] << std::endl;
        return;
    }
    ff.rollForce = 0.0f;  // 	ForceBack[2][0];  //=> desactivated for now !!

    std::cout<<" tipForce = "<<std::fixed << ff.tipForce[0] <<" " << ff.tipForce[1] <<" " <<ff.tipForce[2] <<" " <<ff.rollForce<< '\xd';;

    xiTrocarSetForce(0, &ff);
    xiTrocarFlushForces();


}


SOFA_XITACTPLUGIN_API bool isInitialized = false;

SOFA_XITACTPLUGIN_API int initDevice(XiToolDataIHP& /*data*/)
{
    std::cout<<"initDevice called:"<<std::endl;////////////////////////////////////////////////////////////////////

    if (isInitialized) return 0;
    isInitialized = true;

    const char* vendor = getenv("XITACT_VENDOR");
    if (!vendor || !*vendor)
        vendor = "INRIA_Lille";
    xiSoftwareVendor(vendor);

    return 0;
}


SOFA_DECL_CLASS(IHPDriver)

int IHPDriverClass = core::RegisterObject("Driver and Controller of IHP Xitact Device")
        .add< IHPDriver >();


IHPDriver::IHPDriver()
    : Scale(initData(&Scale, 1.0, "Scale","Default scale applied to the Phantom Coordinates. "))
    , forceScale(initData(&forceScale, 0.0001, "forceScale","Default scale applied to the force feedback. "))
    , permanent(initData(&permanent, true, "permanent" , "Apply the force feedback permanently"))
    , indexTool(initData(&indexTool, (int)0,"toolIndex", "index of the tool to simulate (if more than 1). Index 0 correspond to first tool."))
    , graspThreshold(initData(&graspThreshold, 0.2, "graspThreshold","Threshold value under which grasping will launch an event."))
    , showToolStates(initData(&showToolStates, false, "showToolStates" , "Display states and forces from the tool."))
    , testFF(initData(&testFF, false, "testFF" , "If true will add force when closing handle. As if tool was entering an elastic body."))
    , RefreshFrequency(initData(&RefreshFrequency, (int)500,"RefreshFrequency", "Frequency of the haptic loop."))
{
    std::cout<<"IHPDriver::IHPDriver() called:"<<std::endl;/////////////////////////////////////////////////////////

    myPaceMaker = NULL;
    _mstate = NULL;
    this->f_listening.setValue(true);

    noDevice = false;
    graspElasticMode = false;
    findForceFeedback= false;

#ifdef SOFA_DEV
    data.vm_forceFeedback=NULL;
#endif
    data.lcp_forceFeedback=NULL;
}

IHPDriver::~IHPDriver()
{
    std::cerr<<"IHPDriver::~IHPDriver() called:"<<std::endl;/////////////////////////////////////////////////////////
    xiTrocarRelease();
    this->deleteCallBack();
}

void IHPDriver::cleanup()
{
    /*
    	std::cout<<"IHPDriver::cleanup() called:"<<std::endl;/////////////////////////////////////////////////////////

        isInitialized = false;
    	if (permanent.getValue())
    		this->deleteCallBack();
    */

}

void IHPDriver::setLCPForceFeedback(LCPForceFeedback<defaulttype::Vec1dTypes>* ff)
{
    std::cout<<"IHPDriver::setLCPForceFeedback() called:"<<std::endl;/////////////////////////////////////////////////////////
    if(data.lcp_forceFeedback == ff)
    {
        return;
    }

    if(data.lcp_forceFeedback)
        delete data.lcp_forceFeedback;
    data.lcp_forceFeedback=NULL;
    data.lcp_forceFeedback =ff;
    data.lcp_true_vs_vm_false = true;
};

#ifdef SOFA_DEV
void IHPDriver::setVMForceFeedback(VMechanismsForceFeedback<defaulttype::Vec1dTypes>* ff)
{
    std::cout<<"IHPDriver::setVMForceFeedback() called:"<<std::endl;/////////////////////////////////////////////////////////
    if(data.vm_forceFeedback == ff)
    {
        return;
    }

    if(data.vm_forceFeedback)
        delete data.vm_forceFeedback;
    data.vm_forceFeedback =ff;
    data.lcp_true_vs_vm_false=false;

    std::cout<<"IHPDriver::setVMForceFeedback() ok:"<<std::endl;/////////////////////////////////////////////////////////
};
#endif

void IHPDriver::bwdInit()
{
    std::cout<<"IHPDriver::bwdInit() called:"<<std::endl;/////////////////////////////////////////////////////////
    simulation::Node *context = dynamic_cast<simulation::Node *>(this->getContext()); // access to current node
    if (dynamic_cast<core::behavior::MechanicalState<Vec1dTypes>*>(context->getMechanicalState()) == NULL)
    {
        this->f_printLog.setValue(true);
        serr<<"ERROR : no MechanicalState<Vec1dTypes> defined... init of IHPDriver faild "<<sendl;
        this->_mstate = NULL;
        return ;
    }
    else
    {
        this->_mstate = dynamic_cast<core::behavior::MechanicalState<Vec1dTypes>*> (context->getMechanicalState());

    }

    LCPForceFeedback<defaulttype::Vec1dTypes> *ff = context->get<LCPForceFeedback<defaulttype::Vec1dTypes>>();

    findForceFeedback = false;
    if(ff)
    {
        this->setLCPForceFeedback(ff);
        findForceFeedback = true;
        sout << "setLCPForceFeedback(ff) ok" << sendl;
    }
    else
    {
#ifdef SOFA_DEV

        VMechanismsForceFeedback<defaulttype::Vec1dTypes> *ff = context->get<VMechanismsForceFeedback<defaulttype::Vec1dTypes>>();
        if(ff)
        {
            this->setVMForceFeedback(ff);
            findForceFeedback = true;
            sout << "setVMForceFeedback(ff) ok" << sendl;
        }
        else
#endif
            std::cout << " Error: no FF found" << std::endl;
    }


    setDataValue();



    if(initDevice(data)==-1)
    {
        noDevice=true;
        std::cout<<"WARNING NO DEVICE"<<std::endl;
    }
    //std::cerr  << "IHPDriver::init() done" << std::endl;

    xiTrocarAcquire();
    char name[1024];
    char serial[16];
    int nbr = this->indexTool.getValue();
    xiTrocarGetDeviceDescription(nbr, name);
    xiTrocarGetSerialNumber(nbr,serial );
    //std::cout << "Tool: " << nbr << std::endl;
    //std::cout << "name: " << name << std::endl;
    //std::cout << "serial: " << serial << std::endl;
    //xiTrocarQueryStates();
    xiTrocarGetState(nbr, &data.restState);
    xiTrocarRelease();

    data.indexTool = nbr;

    std::cout<<" CREATE CALLBACK CALL"<<std::endl;

    if (this->permanent.getValue() && findForceFeedback)
    {
        this->createCallBack();
        std::cout<<" CREATE CALLBACK OK"<<std::endl;
    }
    else
    {
        std::cout<<"no FF found or not permanent so no callback created"<<std::endl;
        this->deleteCallBack();
    }



}


void IHPDriver::setDataValue()
{
    std::cout<<"IHPDriver::setDataValue() called:"<<std::endl;/////////////////////////////////////////////////////////
    data.scale = Scale.getValue();
    data.forceScale = forceScale.getValue();
    data.permanent_feedback = permanent.getValue();
    /*
    Quat q = orientationBase.getValue();
    q.normalize();
    orientationBase.setValue(q);
    data.world_H_baseIHP.set( positionBase.getValue(), q		);
    q=orientationTool.getValue();
    q.normalize();
    data.endIHP_H_virtualTool.set(positionTool.getValue(), q);

    */
}

void IHPDriver::reset()
{
    //std::cout<<"IHPDriver::reset() called:"<<std::endl;/////////////////////////////////////////////////////////
    //this->reinit();
}

void IHPDriver::reinitVisual()
{
    std::cout<<"IHPDriver::reinitVisual() called:"<<std::endl;/////////////////////////////////////////////////////////

}

void IHPDriver::reinit()
{

    std::cout<<"IHPDriver::reinit() called:"<<std::endl;/////////////////////////////////////////////////////////
    this->cleanup();
    this->bwdInit();

    this->reinitVisual();
    //this->updateForce();



    /*

    	if (permanent.getValue() && this->findForceFeedback) //if checkBox is changed
    	{
    		std::cout<<" CREATE CALLBACK CALL"<<std::endl;
    		this->createCallBack();
    		std::cout<<" CREATE CALLBACK OK"<<std::endl;

    	}
    	else
    	{
    		std::cerr<<"deleteCallBack"<<std::endl;
    		this->deleteCallBack();
    	}
    	*/
}


void IHPDriver::updateForce()
{

    std::cout<<"IHPDriver::updateForce() called:"<<std::endl;/////////////////////////////////////////////////////////
    // Quick FF test. Add force when using handle. Like in documentation.
    int tool = indexTool.getValue();
    float graspReferencePoint[3] = { 0.0f, 0.0f, 0.0f };
    float kForceScale = 100.0;
    XiToolForce manualForce = { 0 };

    // Checking either handle is open or not:
    if ( (data.simuState.opening <= 0.1) && (!graspElasticMode)) //Activate
    {
        graspElasticMode = true;
        for (unsigned int i = 0; i < 3; ++i)
            graspReferencePoint[i] = data.simuState.trocarDir[i] * data.simuState.toolDepth;
    }

    if ( (data.simuState.opening > 0.1) && (graspElasticMode)) //Desactivate
    {
        graspElasticMode = false;
        xiTrocarSetForce(tool, &manualForce);
        xiTrocarFlushForces();
    }

    if (graspElasticMode)
    {
        for (unsigned int i = 0; i<3; ++i)
            manualForce.tipForce[i] = (graspReferencePoint[i] - (data.simuState.trocarDir[i] * data.simuState.toolDepth)) * kForceScale;

        if (showToolStates.getValue())
        {
            char toolID[16];
            xiTrocarGetSerialNumber(tool,toolID);
            std::cout << toolID << " => Forces = " << manualForce.tipForce[0] << " | " << manualForce.tipForce[1] << " | " << manualForce.tipForce[2] << std::endl;
        }

        manualForce.rollForce = 1.0f;
        xiTrocarSetForce(tool, &manualForce);
        xiTrocarFlushForces();
    }

    std::cout<<"IHPDriver::updateForce() ended:"<<std::endl;
}


void IHPDriver::displayState()
{
    std::cout<<"IHPDriver::displayState() called:"<<std::endl;/////////////////////////////////////////////////////////
    // simple function print the current device state to the screen.
    char toolID[16];
    xiTrocarGetSerialNumber(indexTool.getValue(),toolID);
    XiToolState state = data.simuState;
    std::cout << toolID
            << " => X = " << state.trocarDir[0]
            << ", Y = "   << state.trocarDir[1]
            << ", Z = "   << state.trocarDir[2]
            << ", Ins = " << state.toolDepth
            << ", Roll(rad) = " << state.toolRoll
            << ", Open = " << state.opening << std::endl;
}

void IHPDriver::handleEvent(core::objectmodel::Event *event)
{
    //std::cout<<"IHPDriver::handleEvent() called:"<<std::endl;//////////////////////////////////////////////////////
    static double time_prev;

    if (dynamic_cast<sofa::simulation::AnimateEndEvent *> (event))
    {
        // force the simulation to be "real-time"
        CTime *timer = new CTime();
        double time = 0.001*timer->getRefTime()* PaceMaker::time_scale; // in sec

        // if the computation time is shorter than the Dt set in the simulation... it waits !
        if ((time- time_prev) < getContext()->getDt() )
        {
            double wait_time = getContext()->getDt() - time + time_prev;
            timer->sleep(wait_time);
        }

        time_prev=time;
    }

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


        // saving informations in class structure.
        data.simuState = state;

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


        if (showToolStates.getValue()) // print tool state
            this->displayState();

        if (testFF.getValue()) // try FF when closing handle
            this->updateForce();


        if(_mstate)
        {
            //Assign the state of the device to the rest position of the device.

            if(_mstate->getSize()>5)
            {
                Data<Vec1dTypes::VecCoord >* dataTrocar = _mstate->write(sofa::core::VecCoordId::restPosition());
                helper::WriteAccessor< Data< Vec1dTypes::VecCoord > > vecXTrocar = dataTrocar;

                vecXTrocar[0].x() = thetaX;
                vecXTrocar[1].x() = thetaZ;
                vecXTrocar[2].x() = state.toolRoll;
                vecXTrocar[3].x() = state.toolDepth*Scale.getValue();
                vecXTrocar[4].x() = state.opening;
                vecXTrocar[5].x() = state.opening;
            }
            else
            {
                this->f_printLog.setValue(true);
                serr<<"PROBLEM WITH MSTATE SIZE: must be >= 6"<<sendl;
            }

        }

        // Button and grasp handling event
        XiStateFlags stateFlag;
        stateFlag = state.flags - data.restState.flags;
        if (stateFlag == XI_ToolButtonLeft)
            this->leftButtonPushed();
        else if (stateFlag == XI_ToolButtonRight)
            this->rightButtonPushed();

        if (state.opening < graspThreshold.getValue())
        {
            this->graspClosed();
        }

    }

    //std::cout<<"IHPDriver::handleEvent() ended:"<<std::endl;
}

void IHPDriver::onKeyPressedEvent(core::objectmodel::KeypressedEvent *kpe)
{
    std::cout<<"IHPDriver::onKeyPressedEvent() called:"<<std::endl;/////////////////////////////////////////////////////////
    (void)kpe;


}

void IHPDriver::onKeyReleasedEvent(core::objectmodel::KeyreleasedEvent *kre)
{
    std::cout<<"IHPDriver::onKeyReleasedEvent() called:"<<std::endl;/////////////////////////////////////////////////////////
    (void)kre;
}





Quat IHPDriver::fromGivenDirection( Vector3& dir,  Vector3& local_dir, Quat old_quat)
{
    std::cout<<"IHPDriver::fromGivenDirection() called:"<<std::endl;/////////////////////////////////////////////////////////
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


void IHPDriver::createCallBack()
{

    std::cerr<<"IHPDriver::createCallBack() called:"<<std::endl;/////////////////////////////////////////////////////////
    if (myPaceMaker)
        delete myPaceMaker;
    myPaceMaker=NULL;

    myPaceMaker = new sofa::component::controller::PaceMaker(RefreshFrequency.getValue());
    myPaceMaker->pToFunc =  &UpdateForceFeedBack;
    myPaceMaker->Pdata = &data;
    myPaceMaker->createPace();


    //This function create a thread calling stateCallBack() at a given frequence
    std::cout<<"IHPDriver::createCallBack() ok:"<<std::endl;/////////////////////////////////////////////////////////
}


void IHPDriver::deleteCallBack()
{
    std::cerr<<"IHPDriver::deleteCallBack() called:"<<std::endl;/////////////////////////////////////////////////////////
    if (myPaceMaker)
    {
        delete myPaceMaker;
        myPaceMaker = NULL;
    }
}


void IHPDriver::stateCallBack()
{
    std::cout<<"IHPDriver::stateCallBack() called:"<<std::endl;/////////////////////////////////////////////////////////
    // this function delete thread
}

void IHPDriver::rightButtonPushed()
{
    std::cout<<"IHPDriver::rightButtonPushed() called:"<<std::endl;/////////////////////////////////////////////////////////
    this->operation = true;
}

void IHPDriver::leftButtonPushed()
{
    std::cout<<"IHPDriver::leftButtonPushed() called:"<<std::endl;/////////////////////////////////////////////////////////
    this->operation = false;
}

void IHPDriver::graspClosed()
{
    std::cout<<"IHPDriver::graspClosed() called:"<<std::endl;/////////////////////////////////////////////////////////
    if (operation)//Right pedal operation
    {
        return;
    }
    else //Left pedal operation
        return;
}





} // namespace controller

} // namespace component

} // namespace sofa
