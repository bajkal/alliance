/*
   ### -------------------------------------------------- ### 
   $Author: hcl $
   $Date: 2002/04/25 13:41:34 $
   $Log: ocrConnectorUtil.c,v $
   Revision 1.3  2002/04/25 13:41:34  hcl
   New ripup/reroute loop, bug-kill (CALU&TALU).

   Revision 1.2  2002/03/20 13:25:57  hcl
   SymX bug.

   Revision 1.1  2002/03/15 14:37:26  hcl
   Ca roule.

   Revision 1.1  2002/03/15 09:54:24  hcl
   First commit in the new tree.

   Revision 1.10  2002/01/14 10:34:35  hcl
   OCR should be MBK_SCALE_X - compliant

   Revision 1.9  2001/12/20 15:44:22  hcl
   ALU5 et ALU6 : distance min respectee.

   Revision 1.8  2001/12/20 13:04:04  hcl
   Cosmetique.

   Revision 1.7  2001/12/19 14:22:49  hcl
   Moult petits changements.

   Revision 1.6  2001/12/14 15:58:40  hcl
   indent *.c, quelques bugs en moins, non-placement des connecteurs.

   Revision 1.5  2001/12/12 14:51:09  hcl
   Prise en compte du placement des connecteurs par OCP.

   Revision 1.4  2001/12/10 13:09:03  hcl
   Un bigbug en moins.

   Revision 1.3  2001/12/03 14:31:24  hcl
   Pour la route.

   Revision 1.2  2001/11/20 09:42:06  hcl
   last but not least

   Revision 1.1  2001/09/26 14:27:56  hcl
   premier commit....


   ### -------------------------------------------------- ###
 */

#include <stdlib.h>
#include <string.h>
#include "mut.h"
#include "mlo.h"
#include "mlu.h"
#include "mph.h"
#include "mpu.h"
#include "ocr.h"
#include "ocrUtil.h"
#include "ocrWRoutingDataBase.h"
#include "ocrWindow.h"
#include "ocrPath.h"
#include "display.h"

#include "ocrConnectorUtil.h"

extern ocrOption *g_pOption;

#define LEVEL (g_pOption->LEVEL)

#define MAX_HT 500
#define PITCH   5

ocrNaturalInt
distBetween2VirCon(ocrVirtualConnector * l_pVirCon1,
                   ocrVirtualConnector * l_pVirCon2)
{
    return ABSDIFF(l_pVirCon1->X, l_pVirCon2->X) +
        ABSDIFF(l_pVirCon1->Y, l_pVirCon2->Y);
}


/**  XXX HCl 
*
*  protection des connecteurs qui n'ont plus qu'un seul
*  connecteur virtuel libre
*
**/

//void
//connectorProtect (ocrConnector * i_pCon)
//
//{
//  ocrVirtualConnector *l_pVirConList;
//
//  for (l_pCon = i_pCon ;
//       l_pCon ; l_pCon = l_pCon->NEXT)
//    {
//       if (l_pCon)
//
//    }
//}
//


void countFreeVC(ocrRoutingDataBase * i_pDataBase)
{

    ocrNaturalInt i;
    ocrWRoutingGrid *l_pGrid = i_pDataBase->GRID;

    // tous les sigs
    for (i = 0; i < i_pDataBase->NB_SIGNAL; i++) {
        ocrConnector *l_pCon;

        if (i_pDataBase->SIGNAL[i]->ROUTED
            || i_pDataBase->SIGNAL[i]->NICHT_ZU_ROUTIEREN)
            continue;

        // tous les cons
        for (l_pCon = i_pDataBase->SIGNAL[i]->CON_LIST; l_pCon;
             l_pCon = l_pCon->NEXT) {
            ocrVirtualConnector *l_pVirCon;
            ocrWSegment *l_pSeg;
            if (l_pCon->critVC) {
                protectVC(i_pDataBase, l_pCon->critVC);
                //continue;
                goto fin_lpcon;
            }
            // tous les vircons
            l_pCon->NB_VC = 0;
            for (l_pVirCon = l_pCon->VIR_CON_LIST; l_pVirCon;
                 l_pVirCon = l_pVirCon->NEXT) {
                if (l_pVirCon->Z > 0)
                    goto fin_lpcon;
                    //continue;
                //      l_pSeg = getWSegment (l_pGrid, l_pVirCon->X, l_pVirCon->Y, l_pVirCon->Z - 1);
                //else
                l_pSeg = getWSegmentCV(l_pGrid, l_pVirCon);
                if (l_pSeg->SIGNAL_INDEX == WSEGMENT_FREE) {
                    l_pCon->critVC = l_pVirCon;
                    (l_pCon->NB_VC)++;
                }
            }

            if (l_pCon->NB_VC == 1) {
                protectVC(i_pDataBase, l_pCon->critVC);
            } else {
                l_pCon->critVC = NULL;
            }
          fin_lpcon:
        }
    }
// maj con->NB_VC


    return;
}




/**
 * supprime le connecteur virtuel i_pConVir de 
 * la liste io_pVirConList
**/
void
deleteConVirInConVirList(ocrVirtualConnector ** io_VirConList,
                         ocrVirtualConnector * i_pConVir)
{
    ocrVirtualConnector *l_pVirConList;
    ocrVirtualConnector *l_pVirConListOld = NULL;

    for (l_pVirConList = *io_VirConList;
         l_pVirConList; l_pVirConList = l_pVirConList->NEXT) {
        if (l_pVirConList == i_pConVir) {
            if (l_pVirConListOld == NULL) {
                *io_VirConList = l_pVirConList->NEXT;
            } else {
                l_pVirConListOld->NEXT = l_pVirConList->NEXT;
            }
        }
        l_pVirConListOld = l_pVirConList;
    }
}

/**
 * segment HORIZONTAL
**/

// FIXME
ocrNaturalInt isFreeLine(       /*ocrConnector i_pCon, */
                            ocrWRoutingGrid * i_pGrid,
                            ocrVirtualConnector * l_pVirCon1,
                            ocrVirtualConnector * l_pVirCon2)
{
    ocrWSegment *l_pSegment;
    ocrWSegment *l_pSegment2;

    l_pSegment =
        getWSegment(i_pGrid, l_pVirCon1->X /*/ SCALE_X / PITCH */ ,
                    l_pVirCon1->Y /*/ SCALE_X / PITCH */ ,
                    l_pVirCon1->Z);

    l_pSegment2 =
        getWSegment(i_pGrid, l_pVirCon2->X /*/ SCALE_X / PITCH */ ,
                    l_pVirCon2->Y /*/ SCALE_X / PITCH */ ,
                    l_pVirCon2->Z);

    if ((l_pSegment == l_pSegment2) &&
        (l_pSegment->SIGNAL_INDEX == WSEGMENT_FREE)) {
        return 1;
    }

    return 0;
}

ocrNaturalInt
isFreePoint(ocrConnector * i_pCon,
            ocrWRoutingGrid * i_pGrid, ocrVirtualConnector * l_pVirCon)
// FIXME
{
    ocrWSegment *seg;
    if (l_pVirCon->Z)
        seg = getWSegment(i_pGrid, l_pVirCon->X /* / SCALE_X / PITCH */ ,
                          l_pVirCon->Y /* / SCALE_X / PITCH */ ,
                          l_pVirCon->Z - 1);
    else
        seg = getWSegment(i_pGrid, l_pVirCon->X /* / SCALE_X / PITCH */ ,
                          l_pVirCon->Y /* / SCALE_X / PITCH */ ,
                          l_pVirCon->Z);

    if (seg->SIGNAL_INDEX == WSEGMENT_FREE)
        return 1;

//      if (seg->NAME)
//              if (instr(seg->NAME, i_pCon->NAME, '\0'))
//                      return 1;

    //printf ("Pas libre : (%ld, %ld, %ld)\n", l_pVirCon->X, l_pVirCon->Y, l_pVirCon->Z);


    return 0;

}

/**
 * cr�ation d'un table de hash
 * correspondance : nom de l'instance <-> ptr sur l'instance
**/
void
createHashTable(ocrRoutingDataBase * i_pDataBase, phfig_list * i_pPhFig)
{
    phins_list *l_pInst;

    // Init Tables de HASH
    i_pDataBase->HTABLE = addht(MAX_HT);
    display(LEVEL, DEBUG, "%s", "o HashTable initialization...");

    // parcours des instances physiques
    // sauvegarde des pointeurs dans la table de Hash

    for (l_pInst = i_pPhFig->PHINS; l_pInst; l_pInst = l_pInst->NEXT) {
        addhtitem(i_pDataBase->HTABLE,
                  (void *) l_pInst->INSNAME, (int) (l_pInst));
    }

    display(LEVEL, DEBUG, "%s\n", " done");
}

/**
 * suppression de la table de hash
**/
void deleteHashTable(ocrRoutingDataBase * i_pDataBase)
{
    delht(i_pDataBase->HTABLE);
    i_pDataBase->HTABLE = NULL;
    display(LEVEL, DEBUG, "%s\n", "o Delete HashTable ...");
}


#if 0
//xtof --- pas utilise
/**
 * renvoie les coordonn�es absolue du connecteur en tenant
 * compte des transformations de l'instance
**/
void
getPhConCoord(phins_list * i_pPhIns,
              phfig_list * i_pPhModel,
              phref_list * i_pPhRef,
              ocrNaturalInt * o_uXAbsCon, ocrNaturalInt * o_uYAbsCon)
{
    switch (i_pPhIns->TRANSF) {
    case NOSYM:
        {
            *o_uXAbsCon = i_pPhIns->XINS + i_pPhRef->XREF;
            *o_uYAbsCon = i_pPhIns->YINS + i_pPhRef->YREF;
            break;
        }
    case SYM_Y:
        {
            *o_uXAbsCon = i_pPhIns->XINS + i_pPhRef->XREF;
            *o_uYAbsCon =
                i_pPhIns->YINS + i_pPhModel->YAB2 - i_pPhRef->YREF;
            break;
        }
    case SYM_X:
        {
            *o_uXAbsCon =
                i_pPhIns->XINS + i_pPhModel->XAB2 - i_pPhRef->XREF;
            *o_uYAbsCon = i_pPhIns->YINS + i_pPhRef->YREF;
            break;
        }
    case SYMXY:
        {
            *o_uXAbsCon =
                i_pPhIns->XINS + i_pPhModel->XAB2 - i_pPhRef->XREF;
            *o_uYAbsCon =
                i_pPhIns->YINS + i_pPhModel->YAB2 - i_pPhRef->YREF;
            break;
        }
    default:
        {
            puts("Error getPhConCoord");
            exit(1);
        }
    }
}
#endif

ocrNaturalShort
chooseInternalConnectorLine(ocrWRoutingGrid * i_pGrid,
                            ocrConnector * i_pCon, ocrConnector * i_pCon2)
{
    ocrVirtualConnector *l_pVirCon;
    ocrVirtualConnector *l_pVirCon2;
    ocrNaturalShort l_bOk = 0;

    i_pCon->CON = NULL;
    i_pCon2->CON = NULL;

    // recherche de connecteurs virtuels sur une meme rang�e
    // non utilis�e des 2 instances physiques
    for (l_pVirCon = i_pCon->VIR_CON_LIST;
         l_pVirCon && !l_bOk;
         l_pVirCon = (ocrVirtualConnector *) l_pVirCon->NEXT) {
        for (l_pVirCon2 = i_pCon2->VIR_CON_LIST;
             l_pVirCon2 && !l_bOk;
             l_pVirCon2 = (ocrVirtualConnector *) l_pVirCon2->NEXT) {
            if ((l_pVirCon2->Y == l_pVirCon->Y) &&
                (isFreeLine(i_pGrid, l_pVirCon, l_pVirCon2))) {
                if ((l_pVirCon2->TAG == 0) && (l_pVirCon->TAG == 0)) {
                    l_bOk = 1;
                    i_pCon->CON = l_pVirCon;
                    i_pCon2->CON = l_pVirCon2;

                }
            }
        }
    }

    // Echec, les deux connecteurs ne peuvent pas etre
    // sur la meme ligne. On recherche les deux connecteurs les
    // plus proches et disponibles ...

    if (!i_pCon->CON) {
        ocrNaturalInt l_uDistMin = OCRNATURALINT_MAX;
        ocrNaturalInt l_uDist;

        for (l_pVirCon = i_pCon->VIR_CON_LIST;
             l_pVirCon;
             l_pVirCon = (ocrVirtualConnector *) l_pVirCon->NEXT) {
            if ((isFreePoint(i_pCon, i_pGrid, l_pVirCon))
                && (l_pVirCon->TAG == 0)) {
                for (l_pVirCon2 = i_pCon2->VIR_CON_LIST; l_pVirCon2;
                     l_pVirCon2 =
                     (ocrVirtualConnector *) l_pVirCon2->NEXT) {

                    /*
                       if ((isFreePoint (i_pGrid, l_pVirCon)) &&
                       (isFreePoint (i_pGrid, l_pVirCon2)))
                       {
                       if ((l_pVirCon2->TAG == 0) &&
                       (l_pVirCon->TAG == 0))
                     */

                    if ((isFreePoint(i_pCon2, i_pGrid, l_pVirCon2)) &&
                        (l_pVirCon2->TAG == 0)) {
                        l_uDist =
                            distBetween2VirCon(l_pVirCon, l_pVirCon2);

                        if (l_uDist < l_uDistMin) {
                            l_uDistMin = l_uDist;
                            i_pCon->CON = l_pVirCon;
                            i_pCon2->CON = l_pVirCon2;
                        }
                    }
//                else
                    //                  printf (".");

                }
            }
        }
    }

    if (i_pCon->CON != NULL) {
        i_pCon->CON->TAG = 1;
        i_pCon2->CON->TAG = 1;
    }

    return (i_pCon->CON != NULL);
}

/**
 * D�termine si un connecteur poss�de tous sauf un de ses connecteurs
 * virtuels de pris. (Urgent � router)
 *
 * retourne 1 : OUI, 0 : NON.
**/
ocrNaturalInt
isCriticalConnector(ocrWRoutingGrid * i_pGrid, ocrConnector * i_pCon)
{
    ocrVirtualConnector *l_pConVir;
    ocrNaturalInt l_uConLibre = 0;

    for (l_pConVir = i_pCon->VIR_CON_LIST;
         l_pConVir; l_pConVir = (ocrVirtualConnector *) l_pConVir->NEXT) {
        if (isFreePoint(i_pCon, i_pGrid, l_pConVir))
            l_uConLibre++;
    }
    return (l_uConLibre <= 2);
}

/**
 * Choisit 1 connecteur dans chacune des listes i_pCon i_pCon2
 *
 * Return
 *  OCR_OK 			: OK
 *  OCR_BAD_CON1 	: si l_uCon = 0
 *  OCR_BAD_CON2 	: si l_uCon2 = 0
**/
ocrNaturalShort
chooseInternalConnector(ocrWRoutingGrid * i_pGrid,
                        ocrConnector * i_pCon, ocrConnector * i_pCon2,
                        int mode)
{
    ocrVirtualConnector *l_pVirCon;
    ocrVirtualConnector *l_pVirCon2;
    ocrNaturalInt l_uDistMin = OCRNATURALINT_MAX;
    ocrNaturalInt l_uDist;
    ocrNaturalInt l_uCon = 0;
    ocrNaturalInt l_uCon2 = 0;

    if (!IS_MARK_AS_LINKED(i_pCon))
        i_pCon->CON = NULL;
    if (!IS_MARK_AS_LINKED(i_pCon2))
        i_pCon2->CON = NULL;

    if (mode == 1) {
        if (i_pCon2->VIR_CON_LIST) {
            if (i_pCon2->VIR_CON_LIST->NEXT == NULL) {
                mode = 3;
            } else {
                mode = 0;
            }
        }
    }



    for (l_pVirCon = i_pCon->VIR_CON_LIST;
         l_pVirCon; l_pVirCon = (ocrVirtualConnector *) l_pVirCon->NEXT) {
        if (((isFreePoint(i_pCon, i_pGrid, l_pVirCon)))
            && (l_pVirCon->TAG == 0)) {
            l_uCon++;


            for (l_pVirCon2 = i_pCon2->VIR_CON_LIST;
                 l_pVirCon2;
                 l_pVirCon2 = (ocrVirtualConnector *) l_pVirCon2->NEXT) {
                if ((isFreePoint(i_pCon2, i_pGrid, l_pVirCon2)) &&
                    (((l_pVirCon2->TAG == 0) && (mode == 0))
                     || ((l_pVirCon2->TAG < 5) && (mode == 3))
                     //||
                     //(mode == 3)
                    )
                    ) {
                    l_uCon2++;
                    l_uDist = distBetween2VirCon(l_pVirCon, l_pVirCon2);

                    if (l_uDist < l_uDistMin) {
                        l_uDistMin = l_uDist;
                        i_pCon->CON = l_pVirCon;
                        i_pCon2->CON = l_pVirCon2;
                    }
                }
//                  else
                //                   printf (".");
            }
        }
    }

    if (i_pCon->CON != NULL)
        i_pCon->CON->TAG = 1;
    if (i_pCon2->CON != NULL)
        //i_pCon2->CON->TAG = 1;
        i_pCon2->CON->TAG++;

//   printf ("con : %ld con2 : %ld\n", l_uCon, l_uCon2);
    if (!l_uCon) {
        display (LEVEL, DEBUG, "\n failed for CON1 : %s\n", i_pCon->NAME);
        return OCR_BAD_CON1;
    }
    if (!l_uCon2) {
        display (LEVEL, DEBUG, "\n failed for CON2 : %s\n", i_pCon2->NAME);
        return OCR_BAD_CON2;
    }
    return OCR_OK;
}

ocrNaturalShort
chooseInternalConnector2(ocrWRoutingGrid * i_pGrid,
                         ocrConnector * i_pCon)
{
    ocrVirtualConnector *l_pVirCon;
    ocrNaturalInt l_uCon = 0;

    if (!IS_MARK_AS_LINKED(i_pCon))
        i_pCon->CON = NULL;


    for (l_pVirCon = i_pCon->VIR_CON_LIST;
         l_pVirCon; l_pVirCon = (ocrVirtualConnector *) l_pVirCon->NEXT) {
        if (((isFreePoint(i_pCon, i_pGrid, l_pVirCon)))
            && (l_pVirCon->TAG == 0)) {
            l_uCon++;
            i_pCon->CON = l_pVirCon;
        }
    }

    if (i_pCon->CON != NULL)
        i_pCon->CON->TAG = 1;

    if (!l_uCon)
        return OCR_BAD_CON1;

    return OCR_OK;
}





/* protection des connecteurs isoles */
void
protectVC(ocrRoutingDataBase * i_pDataBase,
          ocrVirtualConnector * i_pVirCon)
{
    ocrWSegment *l_pSeg;
    ocrWRoutingGrid *l_pGrid = i_pDataBase->GRID;
    int z = 0;

    //if (i_pVirCon->Z > 0)
    //      z = i_pVirCon->Z - 1;
    z = i_pVirCon->Z;

    //l_pSeg = getWSegmentCV (l_pGrid, i_pVirCon);

    //for (z = i_pVirCon->Z ; z <= i_pDataBase->NB_OF_LAYERS ; z++) {

    l_pSeg = getWSegment(l_pGrid, i_pVirCon->X, i_pVirCon->Y, z);
    if (l_pSeg->SIGNAL_INDEX != WSEGMENT_FREE)
        return;
    if (getWSegDirection(i_pDataBase->PARAM, l_pSeg) == ocrHorizontal)
        l_pSeg =
            splitWSegment(i_pDataBase->PARAM, l_pGrid, l_pSeg,
                          i_pVirCon->X, i_pVirCon->X, WSEGMENT_OBSTACLE);
    else
        l_pSeg =
            splitWSegment(i_pDataBase->PARAM, l_pGrid, l_pSeg,
                          i_pVirCon->Y, i_pVirCon->Y, WSEGMENT_OBSTACLE);

    //}


    return;
}


void
unProtectVC(ocrRoutingDataBase * i_pDataBase,
            ocrVirtualConnector * i_pVirCon)
{
    ocrWSegment *l_pSeg, *seg1, *seg2;
    ocrWRoutingGrid *l_pGrid = i_pDataBase->GRID;
    int z = 0, i;

    //if (i_pVirCon->Z > 0)
    //      z = i_pVirCon->Z - 1;
    z = i_pVirCon->Z;

    //for (z = i_pVirCon->Z ; z <= i_pDataBase->NB_OF_LAYERS ; z++) {
    //l_pSeg = getWSegmentCV (l_pGrid, i_pVirCon);
    l_pSeg = getWSegment(l_pGrid, i_pVirCon->X, i_pVirCon->Y, z);


    if (l_pSeg->SIGNAL_INDEX == WSEGMENT_OBSTACLE) {
        //deleteSegment (i_pDataBase->PARAM, l_pGrid, l_pSeg);
        l_pSeg->SIGNAL_INDEX = WSEGMENT_FREE;

        // Fusion des segments libres
        //mergeWSegment (i_pDataBase, l_pGrid, l_pSeg);
        //return;
        if (getWSegDirection(i_pDataBase->PARAM, l_pSeg) == ocrHorizontal) {
            //printf("deprotect (%ld,%ld,%d):", 5 * i_pVirCon->X, 5 * i_pVirCon->Y, z);
            seg1 = getWSegment(l_pGrid, i_pVirCon->X - 1, i_pVirCon->Y, z);
            if (seg1->SIGNAL_INDEX == WSEGMENT_FREE) {
                l_pSeg->P_MIN = seg1->P_MIN;
                //printf ("G%ld;", seg1->P_MIN * 5);
                //mbkfree (seg1);
                //freeWSegment (seg1);
            }

            seg2 = getWSegment(l_pGrid, i_pVirCon->X + 1, i_pVirCon->Y, z);
            if (seg2->SIGNAL_INDEX == WSEGMENT_FREE) {
                l_pSeg->P_MAX = seg2->P_MAX;
                //printf ("D%ld.", seg2->P_MAX * 5);
                //freeWSegment (seg2);
                //freeWSegment (seg2);
                //mbkfree (seg2);
            }
            for (i = l_pSeg->P_MIN; i <= l_pSeg->P_MAX; i++)
                setWGrid(l_pGrid, l_pSeg, i, i_pVirCon->Y, z);
            //printf ("\n");
        } else {
            seg1 = getWSegment(l_pGrid, i_pVirCon->X, i_pVirCon->Y - 1, z);
            if (seg1->SIGNAL_INDEX == WSEGMENT_FREE) {
                l_pSeg->P_MIN = seg1->P_MIN;
                //mbkfree (seg1);
                //freeWSegment (seg1);
            }

            seg2 = getWSegment(l_pGrid, i_pVirCon->X, i_pVirCon->Y + 1, z);
            if (seg2->SIGNAL_INDEX == WSEGMENT_FREE) {
                l_pSeg->P_MAX = seg2->P_MAX;
                //freeWSegment (seg2);
                //mbkfree (seg2);
            }
            for (i = l_pSeg->P_MIN; i <= l_pSeg->P_MAX; i++)
                setWGrid(l_pGrid, l_pSeg, i_pVirCon->X, i, z);


        }


    } else {
        //printf ("non - de - protection !\n");
    }
    //}

    return;
}


/* Marquage de segments */

void
markSegmentAsFree(ocrRoutingDataBase * i_pDataBase, ocrSignal * i_pSignal,
                  ocrNaturalInt INDEX)
{
    ocrWSegment *l_pSeg;
    ocrConnector *l_pCon;
    ocrVirtualConnector *l_pVirCon;
    ocrWRoutingGrid *l_pGrid = i_pDataBase->GRID;
    ocrNaturalInt z;


    for (l_pCon = i_pSignal->CON_LIST; l_pCon; l_pCon = l_pCon->NEXT) {
        for (l_pVirCon = l_pCon->VIR_CON_LIST; l_pVirCon;
             l_pVirCon = l_pVirCon->NEXT) {

            if (l_pVirCon->Z == 0)
                continue;
            z = l_pVirCon->Z - 1;
            l_pSeg =
                getWSegment(i_pDataBase->GRID, l_pVirCon->X, l_pVirCon->Y,
                            z);
            if ((l_pSeg->SIGNAL_INDEX == WSEGMENT_OBSTACLE)
                ) {


                // marquer comme libre
                if (l_pCon->INTEXT == INTERNAL) {
                    l_pSeg->SIGNAL_INDEX = WSEGMENT_FREE;
                    mergeWSegment(i_pDataBase->PARAM, l_pGrid, l_pSeg);
                } else {
                    // EXTERNAL
                    if (getWSegDirection(i_pDataBase->PARAM, l_pSeg) ==
                        ocrHorizontal) {
                        splitWSegment(i_pDataBase->PARAM, l_pGrid,
                                      l_pSeg, l_pVirCon->X, l_pVirCon->X,
                                      WSEGMENT_FREE);
                    } else {
                        splitWSegment(i_pDataBase->PARAM,
                                      l_pGrid,
                                      l_pSeg,
                                      l_pVirCon->Y,
                                      l_pVirCon->Y, WSEGMENT_FREE);
                    }
                }
            }
        }                       // for l_pVirCon
    }                           // for l_pCon

    //printf ("%d segments libere(s)\n", bloum);


    return;
}

void
unMarkSegmentAsFree(ocrRoutingDataBase * i_pDataBase,
                    ocrSignal * i_pSignal, ocrNaturalInt INDEX)
{
    ocrWSegment *l_pSeg;
    ocrConnector *l_pCon;
    ocrVirtualConnector *l_pVirCon;
    ocrNaturalInt z;

    for (l_pCon = i_pSignal->CON_LIST; l_pCon; l_pCon = l_pCon->NEXT) {
        //if (l_pCon->NB_VC <= 1)
        //      continue;
        for (l_pVirCon = l_pCon->VIR_CON_LIST; l_pVirCon;
             l_pVirCon = l_pVirCon->NEXT) {

            if (l_pVirCon->Z == 0)
                continue;

            z = l_pVirCon->Z - 1;
            l_pSeg = getWSegment(i_pDataBase->GRID, l_pVirCon->X, l_pVirCon->Y, z);

            if (l_pSeg->SIGNAL_INDEX == WSEGMENT_FREE) {


                // marquer comme occupe
                
                
                if (l_pCon->INTEXT == INTERNAL) {
                    if (getWSegDirection(i_pDataBase->PARAM, l_pSeg) ==
                        ocrHorizontal) {
                        splitWSegment(i_pDataBase->PARAM, i_pDataBase->GRID,
                                      l_pSeg, l_pVirCon->X, l_pVirCon->X,
                                      WSEGMENT_OBSTACLE);
                    } else {
                        splitWSegment(i_pDataBase->PARAM,
                                      i_pDataBase->GRID,
                                      l_pSeg,
                                      l_pVirCon->Y,
                                      l_pVirCon->Y, WSEGMENT_OBSTACLE);
                    }
                } else {
                    l_pSeg->SIGNAL_INDEX = WSEGMENT_OBSTACLE;
                    mergeWSegment(i_pDataBase->PARAM, i_pDataBase->GRID, l_pSeg);
                }
            }


        }                       // for l_pVirCon
    }                           // for l_pCon

    return;
}



/*
   conversion CALU -> CV
*/
ocrNaturalInt add_calu_cv(ocrConnector * i_pCon, phfig_list * i_pPhModel,
                          phins_list * i_pPhIns, phseg_list * i_pPhSeg,
                          ocrNaturalInt INDEX,
                          ocrRoutingDataBase * i_pDataBase)
{

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, z = 0;
    int i, j;
    int check;

#ifdef OCR_DEBUG
    printf("Checkpoint UN\n");
#endif
    switch (i_pPhSeg->LAYER) {
    case CALU1:
        z = 0;
        break;
    case CALU2:
        z = 1;
        break;
    case CALU3:
        z = 2;
        break;
    case CALU4:
        z = 3;
        break;
    case CALU5:
        z = 4;
        break;
    case CALU6:
        z = 5;
        break;
        //case CALU7:
        //      z = 6;
        //      break;
    default:
        /* rien a faire */
#ifdef OCR_DEBUG
        printf("HOUBA !\n");
#endif
        display(LEVEL, ERROR,
                " o %s : should not be in the interface of %s:%s\n",
                i_pPhSeg->NAME, i_pPhIns->INSNAME, i_pPhModel->NAME,
                i_pCon->NAME);
        //exit (-1);
        return 1;
    }
#ifdef OCR_DEBUG
    printf("Checkpoint DEUX\n");
#endif

    switch (i_pPhIns->TRANSF) {
    case NOSYM:
#ifdef OCR_DEBUG
        printf("NOSYM\n");
#endif
        x1 = (i_pPhIns->XINS - i_pPhModel->XAB1) + i_pPhSeg->X1;
        y1 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhSeg->Y1;
        x2 = (i_pPhIns->XINS - i_pPhModel->XAB1) + i_pPhSeg->X2;
        y2 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhSeg->Y2;

        break;
    case SYM_Y:
#ifdef OCR_DEBUG
        printf("SYM_Y\n");
#endif
        x1 = (i_pPhIns->XINS - i_pPhModel->XAB1) + i_pPhSeg->X1;
        y1 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhModel->YAB2 - i_pPhSeg->Y1;
        x2 = (i_pPhIns->XINS - i_pPhModel->XAB1) + i_pPhSeg->X2;
        y2 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhModel->YAB2 - i_pPhSeg->Y2;
        break;
    case SYM_X:
#ifdef OCR_DEBUG
        printf("SYM_X\n");
#endif
        x1 = (i_pPhIns->XINS) + i_pPhModel->XAB2 - i_pPhSeg->X1;
        y1 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhSeg->Y1;
        x2 = (i_pPhIns->XINS) + i_pPhModel->XAB2 - i_pPhSeg->X2;
        y2 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhSeg->Y2;
        break;
    case SYMXY:
#ifdef OCR_DEBUG
        printf("SYMXY\n");
#endif
        x1 = (i_pPhIns->XINS) + i_pPhModel->XAB2 - i_pPhSeg->X1;
        y1 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhModel->YAB2 - i_pPhSeg->Y1;
        x2 = (i_pPhIns->XINS) + i_pPhModel->XAB2 - i_pPhSeg->X2;
        y2 = (i_pPhIns->YINS - i_pPhModel->YAB1) + i_pPhModel->YAB2 - i_pPhSeg->Y2;
        break;
    default:
        display(LEVEL, ERROR, "o Error add_calu_cv\n");
        exit(1);
    }
#ifdef OCR_DEBUG
    printf("Checkpoint TROIS\n");
#endif
#ifdef OCR_DEBUG
    printf("1/ x1=%d; y1=%d; x2=%d; y2=%d; layer=%d\n", x1, y1, x2, y2, z);
#endif
    switch (i_pPhSeg->TYPE) {
    case LEFT:
//                      y1 = y1 - (i_pPhSeg->WIDTH / 2);
//                      y2 = y2 + (i_pPhSeg->WIDTH / 2);
        //tmp = x1;
        //x1 = x2 ; x2 = tmp;
        break;
    case RIGHT:
//                      y1 = y1 - (i_pPhSeg->WIDTH / 2);
//                      y2 = y1 + (i_pPhSeg->WIDTH / 2);
        break;
    case UP:
//                      x1 = x1 - (i_pPhSeg->WIDTH / 2);
//                      x2 = x2 + (i_pPhSeg->WIDTH / 2);
        break;
    case DOWN:
        //tmp = y1;
        //y1 = y2 ; y2 = tmp;
//                      x1 = x1 - (i_pPhSeg->WIDTH / 2);
//                      x2 = x2 + (i_pPhSeg->WIDTH / 2);
        break;
    default:
        exit(1);
        break;
    }
#ifdef OCR_DEBUG
    printf("Checkpoint QUATRE\n");
#endif

    // conversion des coord
#ifdef OCR_DEBUG
    printf("2/ x1=%d; y1=%d; x2=%d; y2=%d\n", x1, y1, x2, y2);
#endif

    x1 = x1 / (5 * SCALE_X);
    x2 = x2 / (5 * SCALE_X);
    y1 = y1 / (5 * SCALE_X);
    y2 = y2 / (5 * SCALE_X);


#ifdef OCR_DEBUG
    printf("3/ X1=%d; Y1=%d; X2=%d; Y2=%d\n", x1, y1, x2, y2);
#endif
    // connecteurs virtuels


    // BIG FIXME HERE ?
    //if (!( (((z % 2)) && (i_pDataBase->PARAM->EVEN_LAYERS_DIRECTION == ocrHorizontal))
    //      || ((!(z % 2) && (i_pDataBase->PARAM->EVEN_LAYERS_DIRECTION == ocrVertical))) ))
    //{ // layer horiz
    //      ocrNaturalInt tmpx1, tmpx2;
    //      tmpx1 = y1 ; y1 = x1 ; x1 = tmpx1;
    //      tmpx2 = y2 ; y2 = x2 ; x2 = tmpx2;
    //}
    if (x1 > x2) {
        ocrNaturalInt tmp = x2;
        x2 = x1;
        x1 = tmp;
    }
    if (y1 > y2) {
        ocrNaturalInt tmp = y2;
        y2 = y1;
        y1 = tmp;
    }

    check = 1;

    for (i = x1; i <= x2; i++)
        for (j = y1; j <= y2; j++) {
            ocrVirtualConnector *i_pVirCon;

            check = 0;
            i_pVirCon = createVirtualConnector(i, j, z, 0, 0);
            addVirtualConnector(&(i_pCon->VIR_CON_LIST), i_pVirCon);
            (i_pCon->NB_VC)++;

//                      if (z) {
//                              pt = malloc (sizeof (struct ocrPoint));
//                              pt->X = i;
//                              pt->Y = j;
//                              pt->Z = z;
//                              pt->SIGNAL_INDEX = INDEX;
//                              pt->NEXT = i_pDataBase->POINTS;
//                              i_pDataBase->POINTS = pt;
//                      }
#ifdef OCR_DEBUG
            printf("VC for :%s, x=%d, y=%d, z=%d\n", i_pCon->NAME, i, j,
                   z);
#endif

        }


#ifdef OCR_DEBUG
    printf("Checkpoint CINQ\n");
#endif

    return check;               // add_calu_cv
}



/**
 * Cr�ation des connecteurs de la bd
*/
void
initConnectorList(ocrRoutingDataBase * i_pDataBase,
                  phfig_list * i_pPhFig, lofig_list * i_pLoFig)
{
    ocrNaturalInt l_nNbSig;
    losig_list *l_pLoSig;
    ocrConnector *l_pCon;
    locon_list *l_pLoCon;
    chain_list *l_pChain;
    phfig_list *l_pPhModel;
    phins_list *l_pPhInst;
    phseg_list *l_pPhSeg;
    ocrNaturalInt l_uNbExternal = 0;
    ocrNaturalInt check;

    // Cr�ation d'une table de Hash sur les instances physiques
    createHashTable(i_pDataBase, i_pPhFig);

    l_nNbSig = 0;
    for (l_pLoSig = i_pLoFig->LOSIG; l_pLoSig; l_pLoSig = l_pLoSig->NEXT) {
        if ((!isvss(l_pLoSig->NAMECHAIN->DATA)) &&
            (!isvdd(l_pLoSig->NAMECHAIN->DATA))) {
            ocrNaturalInt nb_con;


            l_pChain = getptype(l_pLoSig->USER, (long) LOFIGCHAIN)->DATA;

            // parcours des connecteurs du signal
            nb_con = 0;
            while (l_pChain) {
                // nouveau connecteur
                l_pLoCon = (locon_list *) l_pChain->DATA;

                nb_con++;

                // Instance physique du connecteur
                // ROOT est une lofig ou une loins ?
                if (l_pLoCon->ROOT == i_pLoFig) {       // lofig
                    phcon_list *l_pPhCon = NULL;

                    //l_pCon =
                    //  createConnector (l_pLoCon->NAME, i_pLoFig->NAME,
                    //                   OCR_TYPE_PONC, EXTERNAL, 0,      // FACE
                    //                   0,       // ORDER
                    //                   0);      // NumFMF

                    // On ne cherche pas a placer les connecteurs !

                    l_pCon = createConnector(l_pLoCon->NAME, i_pLoFig->NAME, OCR_TYPE_PONC, EXTERNAL, 0,        // FACE
                                             0, // ORDER
                                             0);        // NumFMF

                    addConnector(i_pDataBase->GSIGNAL[l_nNbSig], l_pCon);

                    for (l_pPhCon = i_pPhFig->PHCON; l_pPhCon;
                         l_pPhCon = l_pPhCon->NEXT) {
                        if (l_pPhCon->NAME == l_pLoCon->NAME) {
                            ocrVirtualConnector *l_pVirCon;

                            //printf ("coucou\n");
                            l_pVirCon =
                                createVirtualConnector(l_pPhCon->XCON /
                                                       (5 * SCALE_X),
                                                       l_pPhCon->YCON /
                                                       (5 * SCALE_X), 1, 0, 0);
                            addVirtualConnector(&(l_pCon->VIR_CON_LIST),
                                                l_pVirCon);
                        }
                    }



                    //l_uNbExternal++;



#ifdef OCR_DEBUG
                    printf("treating external connector :%s\n",
                           l_pLoCon->NAME);
#endif
                } else          // loins

                {
                    l_pPhInst =
                        (phins_list *) gethtitem(i_pDataBase->HTABLE,
                                                 (void *) ((loins_list *)
                                                           l_pLoCon->
                                                           ROOT)->INSNAME);

                    if (l_pPhInst == (phins_list *) EMPTYHT) {
                        display (LEVEL, ERROR,
                                 "cannot find the right instance: %s !\n",
                                 ((loins_list *) l_pLoCon->ROOT)->INSNAME);
                        exit(1);
                    }
#ifdef OCR_DEBUG
                    printf
                        ("treating the connector %s of the instance %s of model %s\n",
                         l_pLoCon->NAME,
                         ((loins_list *) l_pLoCon->ROOT)->INSNAME,
                         ((loins_list *) l_pLoCon->ROOT)->FIGNAME);

#endif

                    // Ajout du connecteur interne
                    l_pCon = createConnector(l_pLoCon->NAME, ((loins_list *) l_pLoCon->ROOT)->INSNAME, OCR_TYPE_PONC, INTERNAL, 0,      // FACE
                                             0, // ORDER
                                             0);        // NumFMF

                    addConnector(i_pDataBase->GSIGNAL[l_nNbSig], l_pCon);
#ifdef OCR_DEBUG
                    printf("treating internal connector :%s\n",
                           l_pLoCon->NAME);
#endif

                    // r�cup�rer les coordonn�es des connecteurs grace
                    // au nom du model
                    l_pPhModel = getphfig(l_pPhInst->FIGNAME, 'P');

                    // parcours des connecteurs du model ...
                    // ajout de tous les connecteurs virutels ...
                    check = 0;


                    //if (!check)
                    for (l_pPhSeg = l_pPhModel->PHSEG; l_pPhSeg;
                         l_pPhSeg = l_pPhSeg->NEXT) {
                        if (l_pPhSeg->NAME == l_pLoCon->NAME) {
                            // ajouter les CV.
                            check = 1;
#ifdef OCR_DEBUG
                            printf("ajout d'un segm en CALU pour :%s\n",
                                   l_pLoCon->NAME);
#endif
                            add_calu_cv(l_pCon, l_pPhModel, l_pPhInst,
                                        l_pPhSeg, l_nNbSig, i_pDataBase);
                        }       // end for

                    }           // end if

                }
                l_pChain = l_pChain->NEXT;
            }
            l_nNbSig++;
            if (nb_con < 2) {
                display (LEVEL, WARNING,
                        "net with only one connector : %s\n",
                        i_pDataBase->GSIGNAL[l_nNbSig - 1]->NAME);
                i_pDataBase->GSIGNAL[l_nNbSig - 1]->NICHT_ZU_ROUTIEREN = 1;
                //exit (-1);
            }
        }
    }

//  dumpConnectors ();

    // un petit peu de m�nage ...
    deleteHashTable(i_pDataBase);
}
