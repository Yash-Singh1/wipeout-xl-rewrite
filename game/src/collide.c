#include "functions.h"
#include "ships.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int    ownShip;
extern int    remoteShip;
extern TrackData *track;
extern short  beamCollide;
extern short  scrapeVol;

int collideMinAlpha;

#include "dynam.h"


void doShipShipCollision(int shipNo, ShipData *shipIndex, Object **shipShapes)
{
    short infrontShip;

    infrontShip = getShipInfrontLAP(shipIndex, (short)shipNo);
    if (infrontShip != -1) {
        Ship2ShipCollision(shipIndex, shipShapes, infrontShip, shipNo);
    }
}
void Ship2ShipCollision(ShipData *shipIndex, Object **shipShapes, int i, int j) {
  int mag;
  VECTOR distance;
  VECTOR posVector;
  int collide;
  VECTOR vc;
  int ownMass;
  int remMass;
  VECTOR ship2ship;
  int dotProduct;
  int angle;
  VECTOR ship1React;
  VECTOR ship2React;
  VECTOR soundPos;
  VECTOR sparkPos;
  distance.vx = (shipIndex[i].ppivot.vx - shipIndex[j].ppivot.vx) >> 3;
  distance.vy = (shipIndex[i].ppivot.vy - shipIndex[j].ppivot.vy) >> 3;
  distance.vz = (shipIndex[i].ppivot.vz - shipIndex[j].ppivot.vz) >> 3;
  mag = GetMagnitude(distance);
  if (mag < 120)
  {
    collide = collideLineToPlane(j, i, shipShapes, shipIndex);
    if (collide == 1)
    {
      if ((j == ownShip) || (j == remoteShip))
        ownMass = shipIndex[j].mass;
      else
        ownMass = shipIndex[j].mass * 2;
      if ((i == ownShip) || (i == remoteShip))
        remMass = shipIndex[i].mass;
      else
        remMass = shipIndex[i].mass * 2;
      vc.vx = ((shipIndex[j].vpivot.vx * ownMass) + (shipIndex[i].vpivot.vx * remMass)) / (ownMass + remMass);
      vc.vy = ((shipIndex[j].vpivot.vy * ownMass) + (shipIndex[i].vpivot.vy * remMass)) / (ownMass + remMass);
      vc.vz = ((shipIndex[j].vpivot.vz * ownMass) + (shipIndex[i].vpivot.vz * remMass)) / (ownMass + remMass);
      ship1React.vx = (vc.vx - shipIndex[j].vpivot.vx) >> 1;
      ship1React.vy = (vc.vy - shipIndex[j].vpivot.vy) >> 1;
      ship1React.vz = (vc.vz - shipIndex[j].vpivot.vz) >> 1;
      ship2React.vx = (vc.vx - shipIndex[i].vpivot.vx) >> 1;
      ship2React.vy = (vc.vy - shipIndex[i].vpivot.vy) >> 1;
      ship2React.vz = (vc.vz - shipIndex[i].vpivot.vz) >> 1;
      shipIndex[j].ppivot.vx -= sar(shipIndex[j].vpivot.vx, 6);
      shipIndex[j].ppivot.vy -= sar(shipIndex[j].vpivot.vy, 6);
      shipIndex[j].ppivot.vz -= sar(shipIndex[j].vpivot.vz, 6);
      shipIndex[i].ppivot.vx -= sar(shipIndex[i].vpivot.vx, 6);
      shipIndex[i].ppivot.vy -= sar(shipIndex[i].vpivot.vy, 6);
      shipIndex[i].ppivot.vz -= sar(shipIndex[i].vpivot.vz, 6);
      shipIndex[j].vpivot.vx = vc.vx + ship1React.vx;
      shipIndex[j].vpivot.vy = vc.vy + ship1React.vy;
      shipIndex[j].vpivot.vz = vc.vz + ship1React.vz;
      shipIndex[i].vpivot.vx = vc.vx + ship2React.vx;
      shipIndex[i].vpivot.vy = vc.vy + ship2React.vy;
      shipIndex[i].vpivot.vz = vc.vz + ship2React.vz;
      ship2ship.vx = shipIndex[i].ppivot.vx - shipIndex[j].ppivot.vx;
      ship2ship.vy = shipIndex[i].ppivot.vy - shipIndex[j].ppivot.vy;
      ship2ship.vz = shipIndex[i].ppivot.vz - shipIndex[j].ppivot.vz;
      shipIndex[i].vpivot.vx += ship2ship.vx << 2;
      shipIndex[i].vpivot.vy += ship2ship.vy << 2;
      shipIndex[i].vpivot.vz += ship2ship.vz << 2;
      shipIndex[j].vpivot.vx -= ship2ship.vx << 2;
      shipIndex[j].vpivot.vy -= ship2ship.vy << 2;
      shipIndex[j].vpivot.vz -= ship2ship.vz << 2;
      shipIndex[j].ppivot.vx += sar(shipIndex[j].vpivot.vx, 6);
      shipIndex[j].ppivot.vy += sar(shipIndex[j].vpivot.vy, 6);
      shipIndex[j].ppivot.vz += sar(shipIndex[j].vpivot.vz, 6);
      shipIndex[i].ppivot.vx += sar(shipIndex[i].vpivot.vx, 6);
      shipIndex[i].ppivot.vy += sar(shipIndex[i].vpivot.vy, 6);
      shipIndex[i].ppivot.vz += sar(shipIndex[i].vpivot.vz, 6);
      if ((shipIndex[i].attr & 0x80) == 0)
      {
        if ((shipIndex[j].attr & 0x80) == 0)
        {
          soundPos.vx = (shipIndex[i].ppivot.vx + shipIndex[j].ppivot.vx) >> 1;
          soundPos.vy = (shipIndex[i].ppivot.vy + shipIndex[j].ppivot.vy) >> 1;
          soundPos.vz = (shipIndex[i].ppivot.vz + shipIndex[j].ppivot.vz) >> 1;
          DamageCollide(130, i);
          DamageCollide(130, j);
          PlayDirectionalNote(0, 0, 30, soundPos);
          sparkPos.vx = shipIndex[i].vpivot.vx >> 6;
          sparkPos.vy = shipIndex[i].vpivot.vy >> 6;
          sparkPos.vz = shipIndex[i].vpivot.vz >> 6;
          AddRandomParticles(particleSlots, 8, &soundPos, &sparkPos);
        }
      }
      shipIndex[i].attr |= 0x80;
      shipIndex[j].attr |= 0x80;
    }
  }
  else
  {
    shipIndex[i].attr &= ~0x80;
    shipIndex[j].attr &= ~0x80;
  }
}


int collideLineToPlane(int shipNo1, int shipNo2, Object **shipShapes, ShipData *shipIndex)
{
    int i;
    int count;
    Poly poly;
    short *coords;
    short primitiveCount;
    VECTOR p1, p2, p3;
    VECTOR a, b, c, d;
    VECTOR shifted;
    int factor2;
    VECTOR factor1;
    VECTOR term1;
    VECTOR res_vec;
    int alpha;
    VECTOR DC, BC, AC;
    VECTOR BD, AB;
    VECTOR AD;
    VECTOR start_vec;
    VECTOR pos_vec;
    VECTOR plane1;
    VECTOR P1P2, P1P3;
    VECTOR vec0, vec1, vec2;
    int angle;
    short high1;
    short low1;
    short low2;
    short high2;
    Object *shipShape;
    ShipData *playerShip;
    char skeletonWorkspace[40];

    playerShip = (ShipData *)(shipNo1 << 4);
    playerShip = (ShipData *)((int)playerShip - shipNo1);
    playerShip = (ShipData *)((int)playerShip << 4);
    playerShip = (ShipData *)((int)playerShip + (int)shipIndex);
    shipShape = shipShapes[playerShip->highResTableIndex];
    SetSkeletonPosition(shipShape->skeleton, playerShip->ppivot.vx, playerShip->ppivot.vy, playerShip->ppivot.vz);
    SetSkeletonDirectionHPR(shipShape->skeleton, playerShip->hdg, playerShip->pitch, playerShip->roll);
    playerShip = (ShipData *)(shipNo2 << 4);
    playerShip = (ShipData *)((int)playerShip - shipNo2);
    playerShip = (ShipData *)((int)playerShip << 4);
    playerShip = (ShipData *)((int)playerShip + (int)shipIndex);
    shipShape = shipShapes[playerShip->highResTableIndex];
    SetSkeletonPosition(shipShape->skeleton, playerShip->ppivot.vx, playerShip->ppivot.vy, playerShip->ppivot.vz);
    SetSkeletonDirectionHPR(shipShape->skeleton, playerShip->hdg, playerShip->pitch, playerShip->roll);
    high2 = playerShip->highResTableIndex;
    low2 = playerShip->lowResTableIndex;
    high1 = shipIndex[shipNo1].highResTableIndex;
    low1 = shipIndex[shipNo1].lowResTableIndex;

    getWorldPoint(&a, shipShapes[high2], shipShapes[low2], 0);
    getWorldPoint(&b, shipShapes[high2], shipShapes[low2], 1);
    getWorldPoint(&c, shipShapes[high2], shipShapes[low2], 2);
    getWorldPoint(&d, shipShapes[high2], shipShapes[low2], 3);

    MinusVector(a, c, &AC);
    MinusVector(b, c, &BC);
    MinusVector(d, c, &DC);
    MinusVector(a, b, &AB);
    MinusVector(b, d, &BD);
    MinusVector(a, d, &AD);

    poly.prim = (Primitive *)shipShapes[low2]->primitives;
    primitiveCount = shipShapes[low2]->primitiveCount;

    for (i = 0; i < primitiveCount; i++) {
        switch (poly.prim->type) {
        case TypeF3:
            coords = poly.f3->coords;
            getWorldPoint(&p1, shipShapes[high1], shipShapes[low1], coords[0]);
            getWorldPoint(&p2, shipShapes[high1], shipShapes[low1], coords[1]);
            getWorldPoint(&p3, shipShapes[high1], shipShapes[low1], coords[2]);
            poly.f3++;
            break;

        case TypeG3:
            coords = poly.g3->coords;
            getWorldPoint(&p1, shipShapes[high1], shipShapes[low1], coords[0]);
            getWorldPoint(&p2, shipShapes[high1], shipShapes[low1], coords[1]);
            getWorldPoint(&p3, shipShapes[high1], shipShapes[low1], coords[2]);
            poly.g3++;
            break;

        case TypeFT3:
            coords = poly.ft3->coords;
            getWorldPoint(&p1, shipShapes[high1], shipShapes[low1], coords[0]);
            getWorldPoint(&p2, shipShapes[high1], shipShapes[low1], coords[1]);
            getWorldPoint(&p3, shipShapes[high1], shipShapes[low1], coords[2]);
            poly.ft3++;
            break;

        case TypeGT3:
            coords = poly.gt3->coords;
            getWorldPoint(&p1, shipShapes[high1], shipShapes[low1], coords[0]);
            getWorldPoint(&p2, shipShapes[high1], shipShapes[low1], coords[1]);
            getWorldPoint(&p3, shipShapes[high1], shipShapes[low1], coords[2]);
            poly.gt3++;
            break;

        default:
            break;
        }

        MinusVector(p1, p2, &P1P2);
        MinusVector(p1, p3, &P1P3);
        CrossProduct(&plane1, P1P2, P1P3);

        for (count = 0; count < 6; count++) {
            switch (count) {
            case 0:
                assign(&start_vec, b);
                assign(&pos_vec, BC);
                break;
            case 1:
                assign(&start_vec, a);
                assign(&pos_vec, AC);
                break;
            case 2:
                assign(&start_vec, d);
                assign(&pos_vec, DC);
                break;
            case 3:
                assign(&start_vec, a);
                assign(&pos_vec, AB);
                break;
            case 4:
                assign(&start_vec, a);
                assign(&pos_vec, AD);
                break;
            case 5:
                assign(&start_vec, b);
                assign(&pos_vec, BD);
                break;
            }

            MinusVector(start_vec, p1, &factor1);
            alpha = ScalarProduct(factor1, plane1) << 12;
            factor2 = ScalarProduct(pos_vec, plane1);
            if (factor2 != 0) {
                alpha = alpha / factor2;

                term1.vx = (alpha * pos_vec.vx) >> 12;
                term1.vy = (alpha * pos_vec.vy) >> 12;
                term1.vz = (alpha * pos_vec.vz) >> 12;

                res_vec.vx = start_vec.vx + term1.vx;
                res_vec.vy = start_vec.vy + term1.vy;
                res_vec.vz = start_vec.vz + term1.vz;

                if (alpha >= 0 && alpha <= 4096) {
                    vec0.vx = res_vec.vx - p1.vx;
                    vec0.vy = res_vec.vy - p1.vy;
                    vec0.vz = res_vec.vz - p1.vz;

                    vec1.vx = res_vec.vx - p2.vx;
                    vec1.vy = res_vec.vy - p2.vy;
                    vec1.vz = res_vec.vz - p2.vz;

                    vec2.vx = res_vec.vx - p3.vx;
                    vec2.vy = res_vec.vy - p3.vy;
                    vec2.vz = res_vec.vz - p3.vz;

                    angle = GetAng(vec0, vec1);
                    angle += GetAng(vec1, vec2);
                    angle += GetAng(vec2, vec0);

                    if (angle >= 30000) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}


void Ship2TrackBase(ShipData *playerShip, Face *facePtr, int alpha) {
    VECTOR reaction;
    SVECTOR collisionVector;
    VECTOR nosePos;
    VECTOR wingPos;
    if (alpha < collideMinAlpha) collideMinAlpha = alpha;
    if (alpha <= 0) {
        PlayDirectionalNote(7, 1, 20, playerShip->ppivot);
        playerShip->vpivot.vx -= playerShip->vpivot.vx >> 3;
        playerShip->vpivot.vy -= playerShip->vpivot.vy >> 3;
        playerShip->vpivot.vz -= playerShip->vpivot.vz >> 3;
        playerShip->vpivot.vx += facePtr->normal.vx;
        playerShip->vpivot.vy += facePtr->normal.vy;
        playerShip->vpivot.vz += facePtr->normal.vz;
        playerShip->vpivot.vx -= (facePtr->normal.vx * alpha) >> 4;
        playerShip->vpivot.vy -= (facePtr->normal.vy * alpha) >> 4;
        playerShip->vpivot.vz -= (facePtr->normal.vz * alpha) >> 4;
    } else if (alpha < 31) {
        playerShip->vpivot.vx += facePtr->normal.vx;
        playerShip->vpivot.vy += facePtr->normal.vy;
        playerShip->vpivot.vz += facePtr->normal.vz;
    }
}


void DroneShip2TrackBase(ShipData *playerShip, Face *facePtr, int alpha)
{
    volatile u_char dummy[56];

    (void)dummy;
    if (alpha < collideMinAlpha) {
        collideMinAlpha = alpha;
    }
    if (alpha <= 0) {
        playerShip->ppivot.vx += facePtr->normal.vx >> 3;
        playerShip->ppivot.vy += facePtr->normal.vy >> 3;
        playerShip->ppivot.vz += facePtr->normal.vz >> 3;
        return;
    }
    if (alpha < 0x1F) {
        playerShip->vpivot.vx += facePtr->normal.vx >> 3;
        playerShip->vpivot.vy += facePtr->normal.vy >> 3;
        playerShip->vpivot.vz += facePtr->normal.vz >> 3;
    }
}


void Ship2Track(ShipData *playerShip, Face *facePtr, Object **shipShapes) {
    VECTOR nosePos,wingPos,direction,toFaceVector,test,facePoint;
    int j,alpha,downTrack,toFace;
    short collided;
    short i,shift;
    TrackSection *trackPtr;
    Face *facePtr2;
    nosePos.vx=playerShip->ppivot.vx+(playerShip->unitVecNose.vx>>3);
    nosePos.vy=playerShip->ppivot.vy+(playerShip->unitVecNose.vy>>3);
    nosePos.vz=playerShip->ppivot.vz+(playerShip->unitVecNose.vz>>3);
    wingPos.vx=playerShip->ppivot.vx-(playerShip->unitVecWing.vx>>4)-(playerShip->unitVecNose.vx>>4);
    wingPos.vy=playerShip->ppivot.vy-(playerShip->unitVecWing.vy>>4)-(playerShip->unitVecNose.vy>>4);
    wingPos.vz=playerShip->ppivot.vz-(playerShip->unitVecWing.vz>>4)-(playerShip->unitVecNose.vz>>4);
    trackPtr=playerShip->nearTrkSect->nextSection.ptr;
    direction.vx=trackPtr->centre.vx-playerShip->nearTrkSect->centre.vx;
    direction.vy=trackPtr->centre.vy-playerShip->nearTrkSect->centre.vy;
    direction.vz=trackPtr->centre.vz-playerShip->nearTrkSect->centre.vz;
    downTrack=direction.vx*playerShip->unitVecNose.vx+direction.vy*playerShip->unitVecNose.vy+direction.vz*playerShip->unitVecNose.vz;
    collided=0;i=0;shift=4;
    if(downTrack<0) playerShip->attr&=~8;
    else playerShip->attr|=8;
    toFaceVector.vx=track->vertices[facePtr->vertex[0]].vx-track->vertices[facePtr->vertex[1]].vx;
    toFaceVector.vy=track->vertices[facePtr->vertex[0]].vy-track->vertices[facePtr->vertex[1]].vy;
    toFaceVector.vz=track->vertices[facePtr->vertex[0]].vz-track->vertices[facePtr->vertex[1]].vz;
    direction.vx=playerShip->nearTrkSect->centre.vx-playerShip->ppivot.vx;
    direction.vy=playerShip->nearTrkSect->centre.vy-playerShip->ppivot.vy;
    direction.vz=playerShip->nearTrkSect->centre.vz-playerShip->ppivot.vz;
    toFace=direction.vx*toFaceVector.vx+direction.vy*toFaceVector.vy+direction.vz*toFaceVector.vz;
    if(toFace>0) {
        facePtr=track->faces+playerShip->nearTrkSect->faceStart;
        for(j=0;j<playerShip->nearTrkSect->faceCount;j++) {
            if(facePtr->flags&1) break;
            playerShip->attr|=32;
            if(facePtr==track->faces+playerShip->nearTrkSect->faceStart || playerShip->nearTrkSect->junction.ptr!=0) {
            facePoint.vx=track->vertices[facePtr->vertex[0]].vx;
            facePoint.vy=track->vertices[facePtr->vertex[0]].vy;
            facePoint.vz=track->vertices[facePtr->vertex[0]].vz;
            alpha=PlaneLineCollision(facePoint,nosePos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&16) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                        }
                    }
                } else if(playerShip->nearTrkSect->prevSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->prevSection.ptr->junction.ptr->flags&16)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                        }
                    }
                } else if(playerShip->nearTrkSect->nextSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->nextSection.ptr->junction.ptr->flags&16)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->junction.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                        }
                    }
                } else if(playerShip->nearTrkSect->flags&8) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                        }
                    }
                } else if(playerShip->nearTrkSect->nextSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->nextSection.ptr->junction.ptr->flags&8)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                    }
                } else {NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;}
            }
            wingPos.vx=playerShip->ppivot.vx-(playerShip->unitVecWing.vx>>shift)-(playerShip->unitVecNose.vx>>shift);
            wingPos.vy=playerShip->ppivot.vy-(playerShip->unitVecWing.vy>>shift)-(playerShip->unitVecNose.vy>>shift);
            wingPos.vz=playerShip->ppivot.vz-(playerShip->unitVecWing.vz>>shift)-(playerShip->unitVecNose.vz>>shift);
            alpha=PlaneLineCollision(facePoint,wingPos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&0x18) {
                    if(CheckPolygonCollision(facePtr,wingPos,alpha,1)==1) {NewWingCollision(playerShip,facePtr,-downTrack,alpha);collided=1;}
                } else if(collided) WingCollision(playerShip,facePtr,-downTrack,alpha);
                else NewWingCollision(playerShip,facePtr,-downTrack,alpha);
            }
            wingPos.vx=playerShip->ppivot.vx+(playerShip->unitVecWing.vx>>shift)-(playerShip->unitVecNose.vx>>shift);
            wingPos.vy=playerShip->ppivot.vy+(playerShip->unitVecWing.vy>>shift)-(playerShip->unitVecNose.vy>>shift);
            wingPos.vz=playerShip->ppivot.vz+(playerShip->unitVecWing.vz>>shift)-(playerShip->unitVecNose.vz>>shift);
            alpha=PlaneLineCollision(facePoint,wingPos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&0x18) {
                    if(CheckPolygonCollision(facePtr,wingPos,alpha,1)==1) {NewWingCollision(playerShip,facePtr,-downTrack,alpha);collided=1;}
                } else if(collided) WingCollision(playerShip,facePtr,-downTrack,alpha);
                else NewWingCollision(playerShip,facePtr,-downTrack,alpha);
            }
            }
            facePtr++;
            if(facePtr->flags&1) break;
        }
    } else {
        playerShip->attr&=~32;
        facePtr=track->faces+playerShip->nearTrkSect->faceStart;
        while(!(facePtr->flags&1)) {facePtr++;i++;}
        do {facePtr++;i++;} while(facePtr->flags&1);
        for(j=i;j<playerShip->nearTrkSect->faceCount;j++,facePtr++) {
            if(playerShip->nearTrkSect->faceCount>3 || playerShip->nearTrkSect->junction.ptr!=0) {
            facePoint.vx=track->vertices[facePtr->vertex[0]].vx;
            facePoint.vy=track->vertices[facePtr->vertex[0]].vy;
            facePoint.vz=track->vertices[facePtr->vertex[0]].vz;
            alpha=PlaneLineCollision(facePoint,nosePos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&16) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                        }
                    }
                } else if(playerShip->nearTrkSect->prevSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->prevSection.ptr->junction.ptr->flags&16)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                        }
                    }
                } else if(playerShip->nearTrkSect->nextSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->nextSection.ptr->junction.ptr->flags&16)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->nextSection.ptr->junction.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                        }
                    }
                } else if(playerShip->nearTrkSect->flags&8) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                    }
                } else if(playerShip->nearTrkSect->nextSection.ptr->junction.ptr!=(TrackSection *)-1 && (playerShip->nearTrkSect->nextSection.ptr->junction.ptr->flags&8)) {
                    if(CheckPolygonCollision(facePtr,nosePos,alpha,1)==1) {
                        NoseCollision(playerShip,facePtr,-downTrack,alpha);collided=1;
                    } else {
                        facePtr2=track->faces+playerShip->nearTrkSect->prevSection.ptr->faceStart;
                        facePtr2+=3;
                        if(CheckPolygonCollision(facePtr2,nosePos,alpha,1)==1) {
                            NoseCollision(playerShip,facePtr2,-downTrack,alpha);collided=1;
                        }
                    }
                } else {NoseCollision(playerShip,facePtr,downTrack,alpha);collided=1;}
            }
            wingPos.vx=playerShip->ppivot.vx-(playerShip->unitVecWing.vx>>shift)-(playerShip->unitVecNose.vx>>shift);
            wingPos.vy=playerShip->ppivot.vy-(playerShip->unitVecWing.vy>>shift)-(playerShip->unitVecNose.vy>>shift);
            wingPos.vz=playerShip->ppivot.vz-(playerShip->unitVecWing.vz>>shift)-(playerShip->unitVecNose.vz>>shift);
            alpha=PlaneLineCollision(facePoint,wingPos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&0x18) {
                    if(CheckPolygonCollision(facePtr,wingPos,alpha,1)==1) {NewWingCollision(playerShip,facePtr,downTrack,alpha);collided=1;}
                } else if(collided) WingCollision(playerShip,facePtr,downTrack,alpha);
                else NewWingCollision(playerShip,facePtr,downTrack,alpha);
            }
            wingPos.vx=playerShip->ppivot.vx+(playerShip->unitVecWing.vx>>shift)-(playerShip->unitVecNose.vx>>shift);
            wingPos.vy=playerShip->ppivot.vy+(playerShip->unitVecWing.vy>>shift)-(playerShip->unitVecNose.vy>>shift);
            wingPos.vz=playerShip->ppivot.vz+(playerShip->unitVecWing.vz>>shift)-(playerShip->unitVecNose.vz>>shift);
            alpha=PlaneLineCollision(facePoint,wingPos,facePtr->normal);
            if(alpha<=0) {
                if(playerShip->nearTrkSect->flags&0x18) {
                    if(CheckPolygonCollision(facePtr,wingPos,alpha,1)==1) {NewWingCollision(playerShip,facePtr,downTrack,alpha);collided=1;}
                } else if(collided) WingCollision(playerShip,facePtr,downTrack,alpha);
                else NewWingCollision(playerShip,facePtr,downTrack,alpha);
            }
            }
        }
    }
    if(beamCollide) BeamCollideReaction(playerShip);
}


void BeamCollideReaction(ShipData *playerShip) {
  VECTOR reaction;
  SVector normal;
  int normalX;
  int normalY;
  int normalZ;
  Face *facePtr;
  reaction.vx = playerShip->vpivot.vx >> 2;
  reaction.vy = playerShip->vpivot.vy >> 2;
  reaction.vz = playerShip->vpivot.vz >> 2;
  playerShip->vpivot.vx -= reaction.vx * 3;
  playerShip->vpivot.vy -= reaction.vy * 3;
  playerShip->vpivot.vz -= reaction.vz * 3;
  playerShip->ppivot.vx -= playerShip->vpivot.vx >> 6;
  playerShip->ppivot.vy -= playerShip->vpivot.vy >> 6;
  playerShip->ppivot.vz -= playerShip->vpivot.vz >> 6;
  facePtr = track->faces + playerShip->nearTrkSect->faceStart;
  while (!(facePtr->flags & 1))
    facePtr++;

  facePtr++;
  normalX = facePtr->normal.vx;
  normal.vx = normalX;
  normalY = facePtr->normal.vy;
  normal.vy = normalY;
  normalZ = facePtr->normal.vz;
  normal.vz = normalZ;
  playerShip->ppivot.vx += normalX;
  playerShip->ppivot.vy += normal.vy;
  playerShip->ppivot.vz += normal.vz;
}


void NewWingCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha) {
    VECTOR collisionPoint;
    VECTOR facePoint;
    VECTOR toFaceVector;
    SVECTOR collisionVector;
    VECTOR reaction;
    VECTOR wingPos;
    playerShip->vpivot.vx += facePtr->normal.vx;
    playerShip->vpivot.vy += facePtr->normal.vy >> 1;
    playerShip->vpivot.vz += facePtr->normal.vz;
    if (direction > 0) {
        wingPos.vx = playerShip->ppivot.vx + (playerShip->unitVecWing.vx >> 4) - (playerShip->unitVecNose.vx >> 4);
        wingPos.vy = playerShip->ppivot.vy + (playerShip->unitVecWing.vy >> 4) - (playerShip->unitVecNose.vy >> 4);
        wingPos.vz = playerShip->ppivot.vz + (playerShip->unitVecWing.vz >> 4) - (playerShip->unitVecNose.vz >> 4);
    } else {
        wingPos.vx = playerShip->ppivot.vx - (playerShip->unitVecWing.vx >> 4) - (playerShip->unitVecNose.vx >> 4);
        wingPos.vy = playerShip->ppivot.vy - (playerShip->unitVecWing.vy >> 4) - (playerShip->unitVecNose.vy >> 4);
        wingPos.vz = playerShip->ppivot.vz - (playerShip->unitVecWing.vz >> 4) - (playerShip->unitVecNose.vz >> 4);
    }
    if (!(playerShip->combatAttr & 2)) {
        if (scrapeVol < 200) scrapeVol += 100;
        if (scrapeVol > 200) scrapeVol = 200;
    }
    reaction.vx = playerShip->vpivot.vx >> 6;
    reaction.vy = playerShip->vpivot.vy >> 6;
    reaction.vz = playerShip->vpivot.vz >> 6;
    AddScrapingParticles(particleSlots, 8, &wingPos, &reaction);
    AddRandomParticles(particleSlots, 8, &wingPos, &reaction);
}


void WingCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha) {
  VECTOR incidence;
  VECTOR bounce;
  VECTOR collisionPoint;
  SVECTOR collisionVector;
  VECTOR reaction;
  VECTOR wingPos;
  int magnitude;
  if (alpha < 0)
  {
    bounce.vx = playerShip->vpivot.vx / abs(alpha);
    bounce.vy = playerShip->vpivot.vy / abs(alpha);
    bounce.vz = playerShip->vpivot.vz / abs(alpha);
    playerShip->vpivot.vx = 0;
    playerShip->vpivot.vy = 0;
    playerShip->vpivot.vz = 0;
    playerShip->vpivot.vx += bounce.vx >> 5;
    playerShip->vpivot.vy += bounce.vy >> 5;
    playerShip->vpivot.vz += bounce.vz >> 5;
  }
  playerShip->vpivot.vx += facePtr->normal.vx * 2;
  playerShip->vpivot.vy += facePtr->normal.vy >> 1;
  playerShip->vpivot.vz += facePtr->normal.vz * 2;
  playerShip->ppivot.vx += playerShip->vpivot.vx >> 6;
  playerShip->ppivot.vy += playerShip->vpivot.vy >> 6;
  playerShip->ppivot.vz += playerShip->vpivot.vz >> 6;
  magnitude = (playerShip->speed >> 5) + 200;
  if (direction > 0)
    playerShip->vhdg += magnitude;
  else
    playerShip->vhdg -= magnitude;
  if (direction > 0)
  {
    wingPos.vx = (playerShip->ppivot.vx + (playerShip->unitVecWing.vx >> 4)) - (playerShip->unitVecNose.vx >> 4);
    wingPos.vy = (playerShip->ppivot.vy + (playerShip->unitVecWing.vy >> 4)) - (playerShip->unitVecNose.vy >> 4);
    wingPos.vz = (playerShip->ppivot.vz + (playerShip->unitVecWing.vz >> 4)) - (playerShip->unitVecNose.vz >> 4);
  }
  else
  {
    wingPos.vx = (playerShip->ppivot.vx - (playerShip->unitVecWing.vx >> 4)) - (playerShip->unitVecNose.vx >> 4);
    wingPos.vy = (playerShip->ppivot.vy - (playerShip->unitVecWing.vy >> 4)) - (playerShip->unitVecNose.vy >> 4);
    wingPos.vz = (playerShip->ppivot.vz - (playerShip->unitVecWing.vz >> 4)) - (playerShip->unitVecNose.vz >> 4);
  }
  PlayDirectionalNote(7, 1, 20, wingPos);
  if (playerShip->combatAttr & 2)
  {
    DamageCollide(129, remoteShip);
  }
  else
    if (!(playerShip->combatAttr & 1))
  {
    DamageCollide(129, ownShip);
  }
  reaction.vx = playerShip->vpivot.vx >> 6;
  reaction.vy = playerShip->vpivot.vy >> 6;
  reaction.vz = playerShip->vpivot.vz >> 6;
  AddRandomParticles(particleSlots, 8, &wingPos, &reaction);
}


void NoseCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha) {
  VECTOR incidence;
  VECTOR bounce;
  SVECTOR collisionVector;
  VECTOR nosePos;
  VECTOR reaction;
  int magnitude;
  if (alpha < 0)
  {
    bounce.vx = playerShip->vpivot.vx / abs(alpha);
    bounce.vy = playerShip->vpivot.vy / abs(alpha);
    bounce.vz = playerShip->vpivot.vz / abs(alpha);
    playerShip->vpivot.vx = 0;
    playerShip->vpivot.vy = 0;
    playerShip->vpivot.vz = 0;
    playerShip->vpivot.vx += bounce.vx >> 5;
    playerShip->vpivot.vy += bounce.vy >> 5;
    playerShip->vpivot.vz += bounce.vz >> 5;
  }
  playerShip->vpivot.vx += facePtr->normal.vx * 2;
  playerShip->vpivot.vy += facePtr->normal.vy >> 1;
  playerShip->vpivot.vz += facePtr->normal.vz * 2;
  playerShip->ppivot.vx += playerShip->vpivot.vx >> 6;
  playerShip->ppivot.vy += playerShip->vpivot.vy >> 6;
  playerShip->ppivot.vz += playerShip->vpivot.vz >> 6;
  magnitude = (playerShip->speed >> 2) + 400;
  if (direction > 0)
    playerShip->vhdg += magnitude;
  else
    playerShip->vhdg -= magnitude;
  nosePos.vx = playerShip->ppivot.vx + (playerShip->unitVecNose.vx >> 3);
  nosePos.vy = playerShip->ppivot.vy + (playerShip->unitVecNose.vy >> 3);
  nosePos.vz = playerShip->ppivot.vz + (playerShip->unitVecNose.vz >> 3);
  PlayDirectionalNote(7, 1, 20, nosePos);
  if (playerShip->combatAttr & 2)
  {
    DamageCollide(129, remoteShip);
    DamageCollide(129, remoteShip);
  }
  else
    if (!(playerShip->combatAttr & 1))
  {
    DamageCollide(129, ownShip);
    DamageCollide(129, ownShip);
  }
  reaction.vx = playerShip->vpivot.vx >> 6;
  reaction.vy = playerShip->vpivot.vy >> 6;
  reaction.vz = playerShip->vpivot.vz >> 6;
  AddRandomParticles(particleSlots, 8, &nosePos, &reaction);
}


int PlaneLineCollision(VECTOR plane_point, VECTOR ship_pos, SVector normal) {
    int dot_product, norm_dot_product;
    int coll_data;

    dot_product = (plane_point.vx - ship_pos.vx) * normal.vx;
    dot_product += (plane_point.vy - ship_pos.vy) * normal.vy;
    dot_product += (plane_point.vz - ship_pos.vz) * normal.vz;
    norm_dot_product = (((-normal.vx) * normal.vx) >> 12) +
                       (((-normal.vy) * normal.vy) >> 12) +
                       (((-normal.vz) * normal.vz) >> 12);
    if ((norm_dot_product == -1) && (dot_product == (int)0x80000000)) { }
    coll_data = dot_product / norm_dot_product;
    return coll_data;
}


void Reflection(VECTOR *incidence, SVector normal) {
  int temp;
  temp = (((((int) normal.vx) * (-incidence->vx)) >> 12) + (((int) (normal.vy * (-incidence->vy))) >> 12)) + (((int) (normal.vz * (-incidence->vz))) >> 12);
  incidence->vx = (((2 * ((int) normal.vx)) * temp) >> 12) + incidence->vx;
  incidence->vy = (((2 * ((int) normal.vy)) * temp) >> 12) + incidence->vy;
  incidence->vz = (((2 * ((int) normal.vz)) * temp) >> 12) + incidence->vz;
}


int AngleOfCollision(SVector normal, VECTOR ship_point) {
    int dot_product;
    int norm_dist, shpoint_dist;
    int dist_product;
    int tmp1, tmp2;

    ship_point.vx = ship_point.vx >> 9;
    ship_point.vy = ship_point.vy >> 9;
    ship_point.vz = ship_point.vz >> 9;
    dot_product = (ship_point.vx * normal.vx) +
                  (ship_point.vy * normal.vy) +
                  (ship_point.vz * normal.vz);
    norm_dist = SquareRoot0((normal.vx * normal.vx) +
                              (normal.vy * normal.vy) +
                              (normal.vz * normal.vz));
    shpoint_dist = SquareRoot0((ship_point.vx * ship_point.vx) +
                                  (ship_point.vy * ship_point.vy) +
                                  (ship_point.vz * ship_point.vz));
    dist_product = shpoint_dist * norm_dist;
    tmp1 = dot_product << 12;
    if (dist_product != 0)
        tmp2 = tmp1 / dist_product;
    else
        tmp2 = 0;
    return tmp2;
}


int CheckPolygonCollision(Face *facePtr, VECTOR pos, int alpha, int side) {
    VECTOR planePoint;
    VECTOR vec0, vec1, vec2, vec3;
    int angle;

    planePoint.vx = pos.vx - ((facePtr->normal.vx * alpha) >> 12);
    planePoint.vy = pos.vy - ((facePtr->normal.vy * alpha) >> 12);
    planePoint.vz = pos.vz - ((facePtr->normal.vz * alpha) >> 12);
    vec0.vx = planePoint.vx - track->vertices[facePtr->vertex[1]].vx;
    vec0.vy = planePoint.vy - track->vertices[facePtr->vertex[1]].vy;
    vec0.vz = planePoint.vz - track->vertices[facePtr->vertex[1]].vz;
    vec1.vx = planePoint.vx - track->vertices[facePtr->vertex[2]].vx;
    vec1.vy = planePoint.vy - track->vertices[facePtr->vertex[2]].vy;
    vec1.vz = planePoint.vz - track->vertices[facePtr->vertex[2]].vz;
    if (side != 1) facePtr++;
    vec2.vx = planePoint.vx - track->vertices[facePtr->vertex[0]].vx;
    vec2.vy = planePoint.vy - track->vertices[facePtr->vertex[0]].vy;
    vec2.vz = planePoint.vz - track->vertices[facePtr->vertex[0]].vz;
    vec3.vx = planePoint.vx - track->vertices[facePtr->vertex[3]].vx;
    vec3.vy = planePoint.vy - track->vertices[facePtr->vertex[3]].vy;
    vec3.vz = planePoint.vz - track->vertices[facePtr->vertex[3]].vz;
    angle = GetAng(vec0, vec2);
    angle += GetAng(vec2, vec3);
    angle += GetAng(vec3, vec1);
    angle += GetAng(vec1, vec0);
    return (angle < 30001) ^ 1;
}


int NewCollideLineToPlane(Object *object, Object **shipShapes, ShipData *shipIndex, int shipNo) {
    int i;
    int count;
   Poly        poly;
   short*      coords;
   short       primitiveCount;

    VECTOR p1, p2, p3, p4;

    VECTOR a, b, c, d;

    VECTOR shifted;

    int factor2;
    VECTOR factor1;
    VECTOR term1;

    VECTOR res_vec;
    int alpha;

    VECTOR DC, BC, AC ;
    VECTOR   BD, AB;
    VECTOR    AD;

    VECTOR start_vec;
    VECTOR pos_vec;

    VECTOR plane1;

    VECTOR P1P2;
    VECTOR P1P3;

    VECTOR vec0, vec1, vec2, vec3;
    int angle;

    short high2;
    short low2;
    int polygonSize = 0;
    int planeSize;

    high2 = shipIndex[shipNo].highResTableIndex;
    low2 = shipIndex[shipNo].lowResTableIndex;

    getWorldPoint(&a, shipShapes[high2], shipShapes[low2],0);
    getWorldPoint(&b, shipShapes[high2], shipShapes[low2],1);
    getWorldPoint(&c, shipShapes[high2], shipShapes[low2],2);
    getWorldPoint(&d, shipShapes[high2], shipShapes[low2],3);

    MinusVector(a, c, &AC);
    MinusVector(b, c, &BC);
    MinusVector(d, c, &DC);
    MinusVector(a, b, &AB);
    MinusVector(b, d, &BD);
    MinusVector(a, d, &AD);

   poly.prim = object->primitives;
   primitiveCount = object->primitiveCount;

    for(i = 0; i < primitiveCount; i++)
    {
    switch (poly.prim->type) {
    case TypeF4:
        coords = poly.f4->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        getWorldPoint(&p4, object, object, coords[3]);
        polygonSize = 4;
        poly.f4++;
        break;
    case TypeG4:
        coords = poly.g4->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        getWorldPoint(&p4, object, object, coords[3]);
        polygonSize = 4;
        poly.g4++;
        break;
    case TypeFT4:
        coords = poly.ft4->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        getWorldPoint(&p4, object, object, coords[3]);
        polygonSize = 4;
        poly.ft4++;
        break;
    case TypeGT4:
        coords = poly.gt4->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        getWorldPoint(&p4, object, object, coords[3]);
        polygonSize = 4;
        poly.gt4++;
        break;
    case TypeF3:
        coords = poly.f3->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        polygonSize = 3;
        poly.f3++;
        break;
    case TypeG3:
        coords = poly.g3->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        polygonSize = 3;
        poly.g3++;
        break;
    case TypeFT3:
        coords = poly.ft3->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        polygonSize = 3;
        poly.ft3++;
        break;
    case TypeGT3:
        coords = poly.gt3->coords;
        getWorldPoint(&p1, object, object, coords[0]);
        getWorldPoint(&p2, object, object, coords[1]);
        getWorldPoint(&p3, object, object, coords[2]);
        polygonSize = 3;
        poly.gt3++;
        break;
    default:
        break;
    }
    count = 0;
    MinusVector(p1, p2, &P1P2);
        MinusVector(p1, p3, &P1P3);

        CrossProduct(&plane1, P1P2, P1P3);
        planeSize = polygonSize;

        for (; count<6; count++)
        {
            switch(count)
            {
                case 0:
                    assign(&start_vec, b);
                    assign(&pos_vec, BC);
                    break;
                case 1:
                    assign(&start_vec, a);
                    assign(&pos_vec, AC);
                    break;
                case 2:
                    assign(&start_vec, d);
                    assign(&pos_vec, DC);
                    break;
                case 3:
                    assign(&start_vec, a);
                    assign(&pos_vec, AB);
                    break;
                case 4:
                    assign(&start_vec, a);
                    assign(&pos_vec, AD);
                    break;
                case 5:
                    assign(&start_vec, b);
                    assign(&pos_vec, BD);
                    break;
            }

            MinusVector(start_vec, p1, &factor1);
            alpha = (ScalarProduct(factor1, plane1) << 12);
            factor2 = (ScalarProduct(pos_vec, plane1) );
            if (factor2 != 0)
            {
                alpha = alpha/factor2;

                term1.vx = ((alpha * pos_vec.vx) >> 12);
                term1.vy = ((alpha * pos_vec.vy) >> 12);
                term1.vz = ((alpha * pos_vec.vz) >> 12);

                res_vec.vx = start_vec.vx + term1.vx;
                res_vec.vy = start_vec.vy + term1.vy;
                res_vec.vz = start_vec.vz + term1.vz;

                if ( (alpha>=0) && (alpha<=4096) )
                {
                    vec0.vx = res_vec.vx - p1.vx;
                    vec0.vy = res_vec.vy - p1.vy;
                    vec0.vz = res_vec.vz - p1.vz;

                    vec1.vx = res_vec.vx - p2.vx;
                    vec1.vy = res_vec.vy - p2.vy;
                    vec1.vz = res_vec.vz - p2.vz;

                    vec2.vx = res_vec.vx - p3.vx;
                    vec2.vy = res_vec.vy - p3.vy;
                    vec2.vz = res_vec.vz - p3.vz;

                    vec3.vx = res_vec.vx - p4.vx;
                    vec3.vy = res_vec.vy - p4.vy;
                    vec3.vz = res_vec.vz - p4.vz;
                    if (planeSize == 3) {
                        angle = GetAng(vec0, vec1);
                        angle += GetAng(vec1, vec2);
                        angle += GetAng(vec2, vec0);
                    } else {
                        angle = GetAng(vec0, vec1);
                        angle += GetAng(vec1, vec2);
                        angle += GetAng(vec2, vec0);
                        angle += GetAng(vec2, vec3);
                    }
                    if (planeSize == 4) {
                        if (angle >= 33000) return 1;
                    } else {
                        if (angle >= 30000) return 1;
                    }
                }

            }
        }
    }
    return 0;
}
