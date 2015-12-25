#include "AIMethods.h"

#pragma warning( disable: 4244 )//ignore dataloss conversion from double to long

char EnemyStateProcessing(Character * Player, Character * Phantom){
	//if they are outside of their attack range
    float distanceByLine = distance(Player, Phantom);
    if (distanceByLine <= Phantom->weaponRange){
		unsigned char AtkID = isAttackAnimation(Phantom->animation_id);
		//attack id will tell up if an attack is coming up soon. if so, we need to prevent going into a subroutine(attack), and wait for attack to fully start b4 entering dodge subroutine

		if (
            //if in an animation where subanimation is not used for hurtbox
            (AtkID == 3 && Phantom->subanimation <= AttackSubanimationActiveDuringHurtbox) ||
            //or animation where it is
            (AtkID == 2 && Phantom->subanimation == AttackSubanimationWindupClosing)
			//TODO and their attack will hit me(their rotation is correct and their weapon hitbox width is greater than their rotation delta)
			//&& (Phantom->rotation)>((Player->rotation) - 3.1) && (Phantom->rotation)<((Player->rotation) + 3.1)
		){
            OverrideLowPrioritySubroutines();
            guiPrint(LocationDetection",0:about to be hit (anim id:%d) (suban id:%d)", Phantom->animation_id, Phantom->subanimation);
            return ImminentHit;
		}
		//windup, attack coming
        //TODO should start to plan an attack now and attack while they;re attacking while avoiding the attack
        else if (AtkID == 1 || (AtkID == 2 && Phantom->subanimation == AttackSubanimationWindup)){
			guiPrint(LocationDetection",0:dont attack, enemy windup");
            return EnemyInWindup;
		}
	}

    //backstab checks. If AI CANNOT be attacked/BS'd, cancel Defense Neural net desicion. Also override attack neural net if can bs.
    unsigned char BackStabStateDetected = BackstabDetection(Player, Phantom, distanceByLine);
    if (BackStabStateDetected){
        guiPrint(LocationDetection",0:backstab detection result %d", BackStabStateDetected);
        //will overwrite strafe subroutine
        OverrideLowPrioritySubroutines();
        if (BackStabStateDetected == 2){
            return InBSPosition;
        } else{
            return EnemyNeutral;
        }
    }

    guiPrint(LocationDetection",0:not about to be hit (in dodge subr st:%d) (enemy animation id:%d) (enemy subanimation id:%d)", subroutine_states[DodgeStateIndex], Phantom->animation_id, Phantom->subanimation);
    return EnemyNeutral;
}

/* ------------- DODGE Actions ------------- */

#define inputDelayForStopDodge 40

void StandardRoll(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    guiPrint(LocationState",1:rolling");
    double angle = angleFromCoordinates(Player->loc_x, Phantom->loc_x, Player->loc_y, Phantom->loc_y);
    angle = fabs(angle - 180.0);
    //angle joystick
    longTuple move = angleToJoystick(angle);
    iReport->wAxisX = move.first;
    iReport->wAxisY = move.second;

    //after the joystick input, press circle to roll but dont hold circle, otherwise we run
    long curTime = clock();
    if (curTime < startTimeDefense + inputDelayForStopDodge){
        guiPrint(LocationState",1:circle");
        iReport->lButtons = circle;
    }

    if (
        (curTime > startTimeDefense + inputDelayForStopDodge + 50) &&
        //if we've compleated the dodge move and we're in animation end state we can end
        ((Player->subanimation == AttackSubanimationRecover) ||
        //or we end if not in dodge type animation id, because we could get hit out of dodge subroutine
        !isDodgeAnimation(Player->animation_id))
       ){
        guiPrint(LocationState",1:end dodge roll");
        subroutine_states[DodgeTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
    }
    guiPrint(LocationState",0:dodge roll\n");
}

#define inputDelayForStopCircle 40

void Backstep(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    guiPrint(LocationState",0:Backstep");
    long curTime = clock();

    if (curTime < startTimeDefense + inputDelayForStopCircle){
        iReport->lButtons = circle;
    }

    if (
        (curTime > startTimeDefense + inputDelayForStopCircle)// &&
        //if we've compleated the dodge move and we're in animation end state we can end
        //(Player->subanimation == AttackSubanimationRecover)// ||
        //or we end if not in dodge type animation id, because we could get hit out of dodge subroutine
        //!isDodgeAnimation(Player->animation_id))
        ){
        guiPrint(LocationState",0:end backstep");
        subroutine_states[DodgeTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
    }
}

#define inputDelayForStopStrafe 800

void CounterStrafe(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    guiPrint(LocationState",0:CounterStrafe");
    long curTime = clock();

    //have to lock on to strafe
    if (curTime < startTimeDefense + 30){
        iReport->lButtons = r3;
        guiPrint(LocationState",1:lockon");
    }
    //need a delay for dark souls to respond
    else if (curTime < startTimeDefense + 60){
        iReport->lButtons = 0;
    }

    else if (curTime < startTimeDefense + inputDelayForStopStrafe){
        //TODO make this strafe in the same direction as the enemy strafe
        iReport->wAxisX = XLEFT;
        iReport->wAxisY = MIDDLE / 2;//3/4 pushed up
    }

    //disable lockon
    else if (curTime < startTimeDefense + inputDelayForStopStrafe + 30){
        iReport->lButtons = r3;
        guiPrint(LocationState",1:un lockon");
    }
    else if (curTime < startTimeDefense + inputDelayForStopStrafe + 60){
        iReport->lButtons = 0;
    }

    //break early if we didnt lock on
    if (curTime > startTimeDefense + inputDelayForStopStrafe + 60 || (!Player->locked_on && curTime > startTimeDefense + 60)){
        guiPrint(LocationState",0:end CounterStrafe");
        subroutine_states[DodgeTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
    }
}

static void MoveUp(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport);

void L1Attack(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    guiPrint(LocationState",0:L1");
    long curTime = clock();

    if (curTime < startTimeDefense + 30){
        double angle = angleFromCoordinates(Player->loc_x, Phantom->loc_x, Player->loc_y, Phantom->loc_y);
        longTuple move = angleToJoystick(angle);
        iReport->wAxisX = move.first;
        iReport->wAxisY = move.second;
        iReport->lButtons = l1;
    }

    if (curTime > startTimeDefense + 30){
        guiPrint(LocationState",0:end L1");
        subroutine_states[DodgeTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
    }
}

#define TimeForR3ToTrigger 50
#define TimeForCameraToRotateAfterLockon 190
#define TimeDeltaForGameRegisterAction 120

//reverse roll through enemy attack and roll behind their back
static void ReverseRollBS(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport, char attackInfo){
    guiPrint(LocationState",0:Reverse Roll BS");
    long curTime = clock();

    //have to lock on to reverse roll
    if (curTime > startTimeDefense && curTime < startTimeDefense + TimeForR3ToTrigger){
        iReport->lButtons = r3;
        guiPrint(LocationState",1:lockon");
    }

    //backwards then circle to roll and omnistep via delockon
    if (curTime > startTimeDefense + TimeForR3ToTrigger + TimeForCameraToRotateAfterLockon &&
        curTime < startTimeDefense + TimeForR3ToTrigger + TimeForCameraToRotateAfterLockon + TimeDeltaForGameRegisterAction){
        iReport->wAxisY = YBOTTOM;//have to do this because reverse roll is impossible on non normal camera angles
        iReport->lButtons = r3 + circle;
        guiPrint(LocationState",1:reverse roll");
    }

    //move towards enemy back. Since the reverse roll ends at a about a 45 degree angle ajacent to their back, move in this way
    //Have to move to the side to get behind the enemys back
    //instead of going directly towards enemy, go at that direction +-10 degrees to make the end angle perpendicular to enemy's rotation
    if (curTime > startTimeDefense + TimeForR3ToTrigger + TimeForCameraToRotateAfterLockon + TimeDeltaForGameRegisterAction){
        float angle = angleFromCoordinates(Player->loc_x, Phantom->loc_x, Player->loc_y, Phantom->loc_y);
        if (Phantom->rotation < angle){
            angle += fmod((Phantom->rotation - angle),90);
        } else{
            angle += fmod((angle - Phantom->rotation), 90);
        }
        longTuple joystickAngles = angleToJoystick(angle);
        iReport->wAxisX = joystickAngles.first;
        iReport->wAxisY = joystickAngles.second;
        guiPrint(LocationState",1:moving to bs");
    }

    //TODO point towards enemy

    if (
        (curTime > startTimeDefense + 3000) ||
        //early enemrgency abort in case enemy attack while we try to go for bs after roll
        ((curTime > startTimeDefense + TimeForR3ToTrigger + TimeForCameraToRotateAfterLockon + TimeDeltaForGameRegisterAction) && (attackInfo == ImminentHit))
        )
    {
        guiPrint(LocationState",0:end ReverseRollBS");
        subroutine_states[DodgeTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
    }
}

//initiate the dodge command logic. This can be either toggle escaping, rolling, or parrying.
void dodge(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport, char attackInfo, unsigned char DefenseChoice){
	if (!inActiveSubroutine()){
		//indicate we are in dodge subroutine
        //special mappings to decide between neural net desicion and logic
        switch (attackInfo){
            //reverse roll on any attack
            case ImminentHit:
                subroutine_states[DodgeTypeIndex] = ReverseRollBSId;
                break;
            //only defines backstab detection handling
            default:
                subroutine_states[DodgeTypeIndex] = DefenseChoice;
                break;
        }
		subroutine_states[DodgeStateIndex] = 1;
		//set time for this subroutine
		startTimeDefense = clock();
	}

    switch (subroutine_states[DodgeTypeIndex]){
        case StandardRollId:
            StandardRoll(Player, Phantom, iReport);
            break;
        case BackstepId:
            Backstep(Player, Phantom, iReport);
            break;
        case CounterStrafeId:
            CounterStrafe(Player, Phantom, iReport);
            break;
        case ReverseRollBSId:
            ReverseRollBS(Player, Phantom, iReport, attackInfo);
            break;
        //should never be reached, since we default if instinct dodging
        default:
            guiPrint(LocationState",0:ERROR Unknown dodge action attackInfo=%d\nDodgeNeuralNetChoice=%d\nsubroutine_states[DodgeTypeIndex]=%d", attackInfo, DefenseChoice, subroutine_states[DodgeTypeIndex]);
            break;
    }
}

/* ------------- ATTACK Actions ------------- */

#define inputDelayForStart 10//if we exit move forward and go into attack, need this to prevent kick
#define inputDelayForKick 40
#define inputDelayForRotateBack 90

static void ghostHit(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    long curTime = clock();

    double angle = angleFromCoordinates(Player->loc_x, Phantom->loc_x, Player->loc_y, Phantom->loc_y);

    //hold attack button for a bit
    if ((curTime < startTimeAttack + inputDelayForKick) && (curTime > startTimeAttack + inputDelayForStart)){
        guiPrint(LocationState",1:r1");
        iReport->lButtons = r1;
    }


    //start rotate back to enemy
    //TODO dont have Player subanimation
    if (true){
        guiPrint(LocationState",1:TOWARDS ATTACK. angle %f Player: (%f, %f), Enemy: (%f,%f)", angle, Player->loc_x, Player->loc_y, Phantom->loc_x, Phantom->loc_y);
        longTuple move = angleToJoystick(angle);
        iReport->wAxisX = move.first;
        iReport->wAxisY = move.second;
    }

	//cant angle joystick immediatly, at first couple frames this will register as kick
    //after timeout, point away from enemy till end of windup
    else if (curTime > startTimeAttack + inputDelayForKick){
        guiPrint(LocationState",1:away");
        angle = fmod((180.0 + angle), 360.0);
        longTuple move = angleToJoystick(angle);
        iReport->wAxisX = move.first;
        iReport->wAxisY = move.second;
	}

	//end subanimation on recover animation
    if (
        (curTime > startTimeAttack + inputDelayForRotateBack + 100) &&
        //if we've compleated the attack move and we're in animation end state we can end
        (Player->subanimation == AttackSubanimationRecover)// ||
        //or we end if not in attack type animation id, because we could get hit out of attack subroutine
        //!isAttackAnimation(Player->animation_id))
    ){
        subroutine_states[AttackStateIndex] = 0;
        subroutine_states[AttackTypeIndex] = 0;
        guiPrint(LocationState",0:end sub ghost hit");
	}
}

static void backStab(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    guiPrint(LocationState",0:backstab");
    long curTime = clock();

    //hold attack button for a bit
    if (curTime < startTimeAttack + 40){
        iReport->lButtons = r1;
    }

    //end subanimation on recover animation
    if (
        (curTime > startTimeAttack + inputDelayForRotateBack) &&
        //if we've compleated the attack move and we're in animation end state we can end
        ((Player->subanimation == AttackSubanimationRecover) ||
        //or we end if not in attack type animation id, because we could get hit out of attack subroutine
        !isAttackAnimation(Player->animation_id))
        ){
        subroutine_states[AttackStateIndex] = 0;
        subroutine_states[AttackTypeIndex] = 0;
        guiPrint(LocationState",0:end backstab");
    }
}

#define inputDelayForStopMove 90

static void MoveUp(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport){
    //if we are not close enough, move towards 
    guiPrint(LocationState",0:move up");
    long curTime = clock();
    if (curTime < startTimeAttack + inputDelayForStopMove){
        longTuple move = angleToJoystick(angleFromCoordinates(Player->loc_x, Phantom->loc_x, Player->loc_y, Phantom->loc_y));
        iReport->wAxisX = move.first;
        iReport->wAxisY = move.second;
    }

    if (curTime > startTimeAttack + inputDelayForStopMove){
        subroutine_states[AttackStateIndex] = 0;
        subroutine_states[AttackTypeIndex] = 0;
        subroutine_states[DodgeStateIndex] = 0;
        subroutine_states[DodgeTypeIndex] = 0;
        guiPrint(LocationState",0:end sub");
    }
}

//initiate the attack command logic. This can be a standard(physical) attack or a backstab.
void attack(Character * Player, Character * Phantom, JOYSTICK_POSITION * iReport, char attackInfo, unsigned char AttackNeuralNetChoice){
	//TODO need timing analysis. Opponent can move outside range during windup

    //procede with subroutine if we are not in one already
    //also special case for asyncronous backstabs
    if (!inActiveSubroutine() || attackInfo == InBSPosition){
        //special mappings to decide between neural net desicion and logic, determine if we want to enter attack subroutine
        switch (attackInfo){
            //we are in a position to bs
            case InBSPosition:
                subroutine_states[AttackTypeIndex] = BackstabId;
                break;
            default:
                subroutine_states[AttackTypeIndex] = AttackNeuralNetChoice;
                break;
        }
        if (subroutine_states[AttackTypeIndex]){
            subroutine_states[AttackStateIndex] = 1;
        }
        //set time for this subroutine
        startTimeAttack = clock();
    }

    //may not actually enter subroutine
    if (inActiveAttackSubroutine()){
        //Differentiate different attack subroutines based on neural net decision
        switch (subroutine_states[AttackTypeIndex]){
            //to move towards the opponent
            case MoveUpId:
                MoveUp(Player, Phantom, iReport);
                break;
            //ghost hits for normal attacks
            case GhostHitId:
                ghostHit(Player, Phantom, iReport);
                break;
            //backstab
            case BackstabId:
                backStab(Player, Phantom, iReport);
                break;
            default:
                guiPrint(LocationState",0:ERROR Unknown attack action attackInfo=%d\nAttackNeuralNetChoice=%d\nsubroutine_states[AttackTypeIndex]=%d", attackInfo, AttackNeuralNetChoice, subroutine_states[AttackTypeIndex]);
                break;
        }
    }
}