/*
   ### -------------------------------------------------- ### 
   $Author: hcl $
   $Date: 2002/04/25 13:41:33 $
   $Log: ocrWPlaneLabeling.c,v $
   Revision 1.2  2002/04/25 13:41:33  hcl
   New ripup/reroute loop, bug-kill (CALU&TALU).

   Revision 1.1  2002/03/15 14:37:22  hcl
   Ca roule.

   Revision 1.1  2002/03/15 09:54:21  hcl
   First commit in the new tree.

   Revision 1.7  2002/02/12 15:14:29  hcl
   *** empty log message ***

   Revision 1.6  2001/12/20 13:04:03  hcl
   Cosmetique.

   Revision 1.5  2001/12/19 14:22:49  hcl
   Moult petits changements.

   Revision 1.4  2001/12/14 15:58:39  hcl
   indent *.c, quelques bugs en moins, non-placement des connecteurs.

   Revision 1.3  2001/12/03 16:45:23  hcl
   Corrections dans ripUp2

   Revision 1.2  2001/12/03 14:31:23  hcl
   Pour la route.

   Revision 1.1  2001/09/26 14:27:51  hcl
   premier commit....

   Revision 1.3  2000/02/26 00:24:09  root
   2 points OK

   Revision 1.2  2000/01/25 15:49:00  root
   *** empty log message ***

   ### -------------------------------------------------- ###
 */

#include "stdlib.h"
#include <assert.h>
#include "mut.h"
#include "mlo.h"
#include "mlu.h"
#include "mph.h"
#include "mpu.h"
#include "ocr.h"
#include "ocrUtil.h"
#include "ocrWRoutingDataBase.h"
#include "ocrWindow.h"
#include "mbk_tree.h"
#include "ocrWRoutingUtil.h"
#include "ocrWSegLabeling.h"
#include "ocrWPlaneLabeling.h"

#define SEGMENT(x)  ( (ocrWSegment *) (x)->DATA )
#define OFFSET(x)   ( (ocrNaturalInt) (x)->KEY )

static char *res_id =
    "$Id: ocrWPlaneLabeling.c,v 1.2 2002/04/25 13:41:33 hcl Exp $";

typedef struct ocrSubWSegment {
    ocrWSegment *SEGMENT;
    ocrNaturalInt P_MIN;
    ocrNaturalInt P_MAX;
} ocrSubWSegment;

typedef enum ocrPlaneLabelingEventType {
    ocrAddSegment,
    ocrDelSegment,
} ocrPlaneLabelingEventType;

typedef struct ocrPlaneLabelingEvent {
    ocrPlaneLabelingEventType TYPE;
    ocrNaturalInt DATE;
    ocrWSegment *SEGMENT;
} ocrPlaneLabelingEvent;

ocrSubWSegment *HEAD_OCRSUBWSEGMENT = NULL;     /* sub segment buffer head */

/** **/

static int offsetCompare(void *first, void *second)
{

    return ((ocrNaturalInt) first == (ocrNaturalInt) second) ?
        0 : (((ocrNaturalInt) first > (ocrNaturalInt) second) ? 1 : -1);
}

/** **/

static int eventCompare(const void *e1, const void *e2)
{

    return (((ocrPlaneLabelingEvent *) e1)->DATE ==
            ((ocrPlaneLabelingEvent *) e2)->
            DATE) ? 0 : ((((ocrPlaneLabelingEvent *) e1)->DATE >
                          ((ocrPlaneLabelingEvent *) e2)->DATE) ? 1 : -1);
}

/** create a sub segment **/

static inline ocrSubWSegment *createSubWSegment(ocrWSegment * segment,
                                                ocrNaturalInt first_point,
                                                ocrNaturalInt second_point)
{
    ocrSubWSegment *pt;
    register int i;

    if (HEAD_OCRSUBWSEGMENT == NULL) {
        pt = (ocrSubWSegment *) mbkalloc(BUFSIZE * sizeof(ocrSubWSegment));
        HEAD_OCRSUBWSEGMENT = pt;
        for (i = 1; i < BUFSIZE; i++) {
            (ocrSubWSegment *) pt->SEGMENT = pt + 1;
            pt++;
        }
        (ocrSubWSegment *) pt->SEGMENT = NULL;
    }

    pt = HEAD_OCRSUBWSEGMENT;
    HEAD_OCRSUBWSEGMENT = (ocrSubWSegment *) HEAD_OCRSUBWSEGMENT->SEGMENT;
    pt->SEGMENT = segment;
    pt->P_MIN = first_point;
    pt->P_MAX = second_point;
    return pt;
}

/** free a sub segment **/

static inline void freeSubWSegment(ocrSubWSegment * sub_segment)
{

    (ocrSubWSegment *) sub_segment->SEGMENT = HEAD_OCRSUBWSEGMENT;
    HEAD_OCRSUBWSEGMENT = sub_segment;
}

/** add a segment to labeled segment list **/

static inline void
addWSegToLabeledWSegList(chain_list ** list, ocrWSegment * segment)
{

    list[segment->LAYER] = addchain(list[segment->LAYER],
                                    (void *) segment);
}

/** delete a segment from labeled segment list to add it to segment garbage
    list **/

static inline chain_list *delWSegFromLabeledWSegList(chain_list ** list,
                                                     chain_list *
                                                     prev_element,
                                                     chain_list * element,
                                                     chain_list * garbage)
{

    if (prev_element == NULL)
        list[((ocrWSegment *) element->DATA)->LAYER] = element->NEXT;
    else
        prev_element->NEXT = element->NEXT;
    element->NEXT = garbage;
    return element;
}

/** add a sub segment chain to sub segment list (sub segments are labeled
    with next delta) **/

static inline void
addSubWSegToNextDeltaSubWSegList(chain_list ** list,
                                 ocrNaturalInt * nb_of_elements,
                                 chain_list * sub_segment_chain)
{
    ocrNaturalInt layer;
    chain_list *pt = sub_segment_chain;

    // XXX pourquoi sub_segment_chain peut etre NULL ?
    assert(sub_segment_chain);
    layer = ((ocrSubWSegment *) sub_segment_chain->DATA)->SEGMENT->LAYER;

    nb_of_elements[layer]++;
    if (sub_segment_chain->NEXT) {
        pt = sub_segment_chain->NEXT;
        nb_of_elements[layer]++;
    }
    pt->NEXT = list[layer];
    list[layer] = sub_segment_chain;
}

/** free sub segment list because next delta has just changed **/

void
freeNextDeltaSubWSegList(chain_list ** list,
                         ocrNaturalInt * nb_of_elements,
                         ocrNaturalInt layer)
{
    ocrNaturalInt i;

    for (i = 0; i <= layer; i++) {
        nb_of_elements[i] = 0;
        while (list[i]) {
            freeSubWSegment((ocrSubWSegment *) list[i]->DATA);
            list[i] = delchain(list[i], list[i]);
        }
    }
}

/** **/

static inline ocrSubWSegment *nextDeltaLeftTopLabels(ocrRoutingParameters *
                                                     param,
                                                     ocrWSegment * segment,
                                                     ocrNaturalInt xtarget,
                                                     ocrNaturalInt ytarget,
                                                     ocrNaturalInt delta,
                                                     ocrNaturalInt *
                                                     next_delta)
{
    rbtree *node = segment->LABELS_LEFT_TOP;
    rbtree *parent = RBTREENULL;
    rbtree *inf = NULL;
    rbtree *sup = NULL;
    ocrNaturalInt point;
    ocrNaturalInt delta_inf;
    ocrNaturalShort pitch =
        (getWSegDirection(param, segment) ==
         ocrHorizontal) ? param->PITCH_H : param->PITCH_V;

    while (node != RBTREENULL) {
        parent = node;
        if (delta == DELTA(node)) {
            inf = node;
            sup = getnextrbtree(node);
            break;
        }
        if (delta < DELTA(node))
            node = node->Left;
        else
            node = node->Right; /* TAG ERROR */
    }

    if (parent == RBTREENULL) {
        *next_delta = OCRNATURALINT_MAX;
        return NULL;
    }

    if (node == RBTREENULL) {
        if (delta < DELTA(parent)) {
            inf = getpreviousrbtree(parent);
            sup = parent;
        } else {
            inf = parent;
            sup = getnextrbtree(parent);
        }
    }

    if (inf == RBTREENULL) {
        *next_delta = DELTA(sup);
        if (getWSegDirection(param, segment) == ocrHorizontal)
            return createSubWSegment(segment,
                                     POINT(sup),
                                     MIN(xtarget, segment->P_MAX));
        else
            return createSubWSegment(segment,
                                     POINT(sup),
                                     MIN(ytarget, segment->P_MAX));
    } else {
        point = -((delta - DELTA(inf)) / (2 * pitch)) + POINT(inf) - 1;
        delta_inf = DELTA(inf) - 2 * pitch * (point - POINT(inf));
        if (sup == RBTREENULL) {        /* TAG UNDERFLOW */
            if ((point < POINT(inf)) && (point >= segment->P_MIN)) {
                *next_delta = delta_inf;
                return createSubWSegment(segment, point, point);
            } else {
                *next_delta = OCRNATURALINT_MAX;
                return NULL;
            }
        } else {
            if (delta_inf < DELTA(sup)) {
                *next_delta = delta_inf;
                return createSubWSegment(segment, point, point);
            } else {
                *next_delta = DELTA(sup);
                return createSubWSegment(segment, POINT(sup), point);
            }
        }
    }
}

/** **/

static inline ocrSubWSegment
    *nextDeltaRightBottomLabels(ocrRoutingParameters * param,
                                ocrWSegment * segment,
                                ocrNaturalInt xtarget,
                                ocrNaturalInt ytarget, ocrNaturalInt delta,
                                ocrNaturalInt * next_delta)
{
    rbtree *node = segment->LABELS_RIGHT_BOTTOM;
    rbtree *parent = RBTREENULL;
    rbtree *inf = NULL;
    rbtree *sup = NULL;
    ocrNaturalInt point;
    ocrNaturalInt delta_inf;
    ocrNaturalShort pitch =
        (getWSegDirection(param, segment) ==
         ocrHorizontal) ? param->PITCH_H : param->PITCH_V;

    while (node != RBTREENULL) {
        parent = node;
        if (delta == DELTA(node)) {
            inf = node;
            sup = getnextrbtree(node);
            break;
        }
        if (delta < DELTA(node))
            node = node->Left;  /* TAG ERROR */
        else
            node = node->Right;
    }

    if (parent == RBTREENULL) {
        *next_delta = OCRNATURALINT_MAX;
        return NULL;
    }

    if (node == RBTREENULL) {
        if (delta < DELTA(parent)) {
            inf = getpreviousrbtree(parent);
            sup = parent;
        } else {
            inf = parent;
            sup = getnextrbtree(parent);
        }
    }

    if (inf == RBTREENULL) {
        *next_delta = DELTA(sup);
        if (getWSegDirection(param, segment) == ocrHorizontal)
            return createSubWSegment(segment,
                                     MAX(xtarget + 1, segment->P_MIN),
                                     POINT(sup));
        else
            return createSubWSegment(segment,
                                     MAX(ytarget + 1, segment->P_MIN),
                                     POINT(sup));
    } else {
        point = (delta - DELTA(inf)) / (2 * pitch) + POINT(inf) + 1;
        delta_inf = DELTA(inf) + 2 * pitch * (point - POINT(inf));
        if (sup == RBTREENULL) {
            if ((point > POINT(inf)) && (point <= segment->P_MAX)) {
                *next_delta = delta_inf;
                return createSubWSegment(segment, point, point);
            } else {
                *next_delta = OCRNATURALINT_MAX;
                return NULL;
            }
        } else {
            if (delta_inf < DELTA(sup)) {
                *next_delta = delta_inf;
                return createSubWSegment(segment, point, point);
            } else {
                *next_delta = DELTA(sup);
                return createSubWSegment(segment, point, POINT(sup));
            }
        }
    }
}

/** **/

static inline chain_list *processNextDeltaInWSeg(ocrRoutingParameters *
                                                 param,
                                                 ocrWSegment * segment,
                                                 ocrNaturalInt xtarget,
                                                 ocrNaturalInt ytarget,
                                                 ocrNaturalInt delta,
                                                 ocrNaturalInt *
                                                 next_delta)
{
    ocrSubWSegment *sub_segment_LT;
    ocrSubWSegment *sub_segment_RB;
    ocrNaturalInt next_delta_LT;
    ocrNaturalInt next_delta_RB;

    sub_segment_LT = nextDeltaLeftTopLabels(param,
                                            segment,
                                            xtarget,
                                            ytarget,
                                            delta, &next_delta_LT);
    sub_segment_RB = nextDeltaRightBottomLabels(param,
                                                segment,
                                                xtarget,
                                                ytarget,
                                                delta, &next_delta_RB);
    if (!sub_segment_LT) {
        *next_delta = next_delta_RB;
        if (!sub_segment_RB)
            return NULL;
        else
            return addchain(NULL, (void *) sub_segment_RB);
    }
    if (!sub_segment_RB) {
        *next_delta = next_delta_LT;
        return addchain(NULL, (void *) sub_segment_LT);
    }
    if (next_delta_LT < next_delta_RB) {
        freeSubWSegment(sub_segment_RB);
        *next_delta = next_delta_LT;
        return addchain(NULL, (void *) sub_segment_LT);
    }
    if (next_delta_LT > next_delta_RB) {
        freeSubWSegment(sub_segment_LT);
        *next_delta = next_delta_RB;
        return addchain(NULL, (void *) sub_segment_RB);
    }
    *next_delta = next_delta_LT;        /* next_delta_LT == next_delta_RB */
    if (sub_segment_LT->P_MAX + 1 == sub_segment_RB->P_MIN) {
        sub_segment_LT->P_MAX = sub_segment_RB->P_MAX;
        freeSubWSegment(sub_segment_RB);
        return addchain(NULL, (void *) sub_segment_LT);
    } else
        return addchain(addchain(NULL, (void *) sub_segment_RB),
                        (void *) sub_segment_LT);
}

/** **/

ocrNaturalInt
processNextDelta(ocrRoutingParameters * param,
                 ocrWRoutingGrid * grid,
                 ocrNaturalInt xtarget,
                 ocrNaturalInt ytarget,
                 ocrNaturalInt delta,
                 chain_list ** labeled_segment_list,
                 chain_list ** next_delta_sub_segment_list,
                 ocrNaturalInt * nb_of_sub_segments,
                 chain_list ** segment_garbage)
{
    chain_list *segment_chain;
    chain_list *prev_segment_chain;
    chain_list *pt;
    chain_list *sub_segment_chain;
    ocrNaturalInt next_delta_current = OCRNATURALINT_MAX;
    ocrNaturalInt next_delta;
    ocrNaturalInt layer;

    freeNextDeltaSubWSegList(next_delta_sub_segment_list,
                             nb_of_sub_segments, grid->NB_OF_LAYERS - 1);
    for (layer = 0; layer < grid->NB_OF_LAYERS; layer++) {
        prev_segment_chain = NULL;
        segment_chain = labeled_segment_list[layer];
        while (segment_chain) {
            sub_segment_chain = processNextDeltaInWSeg(param,
                                                       (ocrWSegment *)
                                                       segment_chain->DATA,
                                                       xtarget, ytarget,
                                                       delta, &next_delta);
            if (!sub_segment_chain) {
                pt = segment_chain->NEXT;
                *segment_garbage =
                    delWSegFromLabeledWSegList(labeled_segment_list,
                                               prev_segment_chain,
                                               segment_chain,
                                               *segment_garbage);
                segment_chain = pt;
            } else {
                if (next_delta < next_delta_current) {
                    next_delta_current = next_delta;
                    freeNextDeltaSubWSegList(next_delta_sub_segment_list,
                                             nb_of_sub_segments, layer);
                }
                if (next_delta == next_delta_current) {
                    addSubWSegToNextDeltaSubWSegList
                        (next_delta_sub_segment_list, nb_of_sub_segments,
                         sub_segment_chain);
                } else {
                    freeSubWSegment((ocrSubWSegment *) sub_segment_chain->
                                    DATA);
                    sub_segment_chain =
                        delchain(sub_segment_chain, sub_segment_chain);
                    if (sub_segment_chain) {
                        freeSubWSegment((ocrSubWSegment *)
                                        sub_segment_chain->DATA);
                        freechain(sub_segment_chain);
                    }
                }
                prev_segment_chain = segment_chain;
                segment_chain = segment_chain->NEXT;
            }
        }
    }
    return next_delta_current;
}

/****************************************************************************/

/**
**/

static inline ocrPlaneLabelingEvent *makeSchedule(chain_list *
                                                  sub_segment_chain_list,
                                                  ocrNaturalInt
                                                  nb_of_events)
{
    ocrPlaneLabelingEvent *schedule =
        (ocrPlaneLabelingEvent *) mbkalloc(nb_of_events *
                                           sizeof(ocrPlaneLabelingEvent));
    ocrNaturalInt i = 0;

    for (; sub_segment_chain_list;
         sub_segment_chain_list = sub_segment_chain_list->NEXT) {
        schedule[i].TYPE = ocrAddSegment;
        schedule[i].DATE =
            ((ocrSubWSegment *) sub_segment_chain_list->DATA)->P_MIN;
        schedule[i].SEGMENT =
            ((ocrSubWSegment *) sub_segment_chain_list->DATA)->SEGMENT;
        i++;
        schedule[i].TYPE = ocrDelSegment;
        schedule[i].DATE =
            ((ocrSubWSegment *) sub_segment_chain_list->DATA)->P_MAX + 1;
        schedule[i].SEGMENT =
            ((ocrSubWSegment *) sub_segment_chain_list->DATA)->SEGMENT;
        i++;
    }
    qsort((void *) schedule,
          (size_t) nb_of_events,
          (size_t) sizeof(ocrPlaneLabelingEvent), eventCompare);
    return schedule;
}

/**
 *
 * Mise � jour des segments du calendrier
 * de schedule_index jusqu'� nb_of_events
 * 
**/

// labeling_line sauvegarde de event_line

static inline rbtree *labelPlaneUpdate(rbtree * labeling_status,
                                       ocrPlaneLabelingEvent *
                                       event_schedule,
                                       ocrNaturalInt * schedule_index,
                                       ocrNaturalInt nb_of_events,
                                       ocrNaturalInt * event_line,
                                       ocrNaturalInt * labeling_line)
{

    *labeling_line = *event_line;       /* update labeling line */

    while ((*schedule_index < nb_of_events) &&  /* update labeling status */
           (event_schedule[*schedule_index].DATE == *event_line)) {
        if (event_schedule[*schedule_index].TYPE == ocrAddSegment)
            labeling_status = addrbtree(labeling_status,
                                        (void *)
                                        event_schedule[*schedule_index].
                                        SEGMENT->OFFSET,
                                        (void *)
                                        event_schedule[*schedule_index].
                                        SEGMENT, offsetCompare);
        else if (event_schedule[*schedule_index].TYPE == ocrDelSegment)
	    if (labeling_status)
		labeling_status = delrbtree(labeling_status,
					    getrbtree(labeling_status,
					  	     (void *)
					    	     event_schedule
					      	     [*schedule_index].
						     SEGMENT->OFFSET,
						     offsetCompare));
        else {
            fprintf(stderr, "OCR internal error : event not ADD nor DEL\n");
            exit(1);
        }

        (*schedule_index)++;
    }

    if (*schedule_index < nb_of_events) /* update event_line */
        *event_line = event_schedule[*schedule_index].DATE;

    return labeling_status;
}

/** **/

static inline void
processNextDeltaAgain(ocrRoutingParameters * param,
                      ocrNaturalInt xtarget,
                      ocrNaturalInt ytarget,
                      ocrNaturalInt delta,
                      chain_list ** next_delta_sub_segment_list,
                      ocrNaturalInt * nb_of_sub_segments,
                      ocrWSegment * segment)
{
    ocrNaturalInt delta_tmp;
    chain_list *sub_segment_chain = processNextDeltaInWSeg(param,
                                                           segment,
                                                           xtarget,
                                                           ytarget,
                                                           delta - 1,
                                                           &delta_tmp);

    addSubWSegToNextDeltaSubWSegList(next_delta_sub_segment_list,
                                     nb_of_sub_segments,
                                     sub_segment_chain);
}

/**
 *
 * 
 * 
**/

static inline void labelPlaneProcess(ocrRoutingParameters * param, ocrWRoutingGrid * grid, ocrNaturalInt xtarget, ocrNaturalInt ytarget, rbtree * labeling_status, ocrNaturalInt event_line, ocrNaturalInt labeling_line,       // ligne courante du scheduler
                                     ocrNaturalInt delta_sce,
                                     ocrNaturalInt delta_dest,
                                     ocrNaturalInt plane_dest,       // z
                                     chain_list ** labeled_segment_list,
                                     chain_list **
                                     next_delta_sub_segment_list,
                                     ocrNaturalInt * nb_of_sub_segments)
{
    ocrWSegment *segment_dest;
    rbtree *first_segment_tree = getmostrbtree(labeling_status);
    rbtree *segment_tree1;
    rbtree *segment_tree2;
    rbtree *previous;
    rbtree *next;

    if (labeling_status == RBTREENULL)
        return;


    // effectue les op�rations jusqu'a la date courante ?
    while (labeling_line < event_line) {
        segment_tree1 = first_segment_tree;

        while (segment_tree1 != RBTREENULL) {
            segment_dest = getWSegment(grid,
                                       getWSegXCoord(param,
                                                     SEGMENT
                                                     (segment_tree1),
                                                     labeling_line),
                                       getWSegYCoord(param,
                                                     SEGMENT
                                                     (segment_tree1),
                                                     labeling_line),
                                       plane_dest);

            segment_tree2 = getrbtree2(labeling_status,
                                       (void *) segment_dest->P_MAX,
                                       &previous, &next, offsetCompare);

            if (!isObstructed(segment_dest)) {

                if (!isLabeled(segment_dest))
                    addWSegToLabeledWSegList(labeled_segment_list,
                                             segment_dest);

                if ((segment_tree2 == RBTREENULL) &&
                    (OFFSET(previous) != OFFSET(segment_tree1)))
                    segment_tree2 = previous;

                if (segment_tree2 == RBTREENULL) {
                    if ((labelWSegment(param,
                                       segment_dest,
                                       xtarget,
                                       ytarget,
                                       delta_dest,
                                       OFFSET(segment_tree1))) &&
                        (delta_dest == delta_sce))
                        processNextDeltaAgain(param,
                                              xtarget,
                                              ytarget,
                                              delta_dest,
                                              next_delta_sub_segment_list,
                                              nb_of_sub_segments,
                                              segment_dest);
                } else {
                    if (getWSegDirection(param, segment_dest) ==
                        ocrHorizontal) {
                        if (xtarget <= OFFSET(segment_tree1)) {
                            if ((labelWSegment(param,
                                               segment_dest,
                                               xtarget,
                                               ytarget,
                                               delta_dest,
                                               OFFSET(segment_tree2))) &&
                                (delta_dest == delta_sce))
                                processNextDeltaAgain(param,
                                                      xtarget,
                                                      ytarget,
                                                      delta_dest,
                                                      next_delta_sub_segment_list,
                                                      nb_of_sub_segments,
                                                      segment_dest);
                        } else {
                            if (xtarget >= OFFSET(segment_tree2)) {
                                if ((labelWSegment(param,
                                                   segment_dest,
                                                   xtarget,
                                                   ytarget,
                                                   delta_dest,
                                                   OFFSET(segment_tree1)))
                                    && (delta_dest == delta_sce))
                                    processNextDeltaAgain(param, xtarget,
                                                          ytarget,
                                                          delta_dest,
                                                          next_delta_sub_segment_list,
                                                          nb_of_sub_segments,
                                                          segment_dest);
                            } else {
                                if (((labelWSegment(param,
                                                    segment_dest,
                                                    xtarget,
                                                    ytarget,
                                                    delta_dest,
                                                    OFFSET(segment_tree1)))
                                     |
                                     /* warning not || ! */
                                     (labelWSegment(param,
                                                    segment_dest,
                                                    xtarget,
                                                    ytarget,
                                                    delta_dest,
                                                    OFFSET
                                                    (segment_tree2))))
                                    && (delta_dest == delta_sce))
                                    processNextDeltaAgain(param, xtarget,
                                                          ytarget,
                                                          delta_dest,
                                                          next_delta_sub_segment_list,
                                                          nb_of_sub_segments,
                                                          segment_dest);
                            }
                        }
                    } else {
                        if (ytarget <= OFFSET(segment_tree1)) {
                            if ((labelWSegment(param,
                                               segment_dest,
                                               xtarget,
                                               ytarget,
                                               delta_dest,
                                               OFFSET(segment_tree2))) &&
                                (delta_dest == delta_sce))
                                processNextDeltaAgain(param,
                                                      xtarget,
                                                      ytarget,
                                                      delta_dest,
                                                      next_delta_sub_segment_list,
                                                      nb_of_sub_segments,
                                                      segment_dest);
                        } else {
                            if (ytarget >= OFFSET(segment_tree2)) {
                                if ((labelWSegment(param,
                                                   segment_dest,
                                                   xtarget,
                                                   ytarget,
                                                   delta_dest,
                                                   OFFSET(segment_tree1)))
                                    && (delta_dest == delta_sce))
                                    processNextDeltaAgain(param, xtarget,
                                                          ytarget,
                                                          delta_dest,
                                                          next_delta_sub_segment_list,
                                                          nb_of_sub_segments,
                                                          segment_dest);
                            } else {
                                if (((labelWSegment(param,
                                                    segment_dest,
                                                    xtarget,
                                                    ytarget,
                                                    delta_dest,
                                                    OFFSET(segment_tree1)))
                                     |
                                     /* warning not || ! */
                                     (labelWSegment(param,
                                                    segment_dest,
                                                    xtarget,
                                                    ytarget,
                                                    delta_dest,
                                                    OFFSET
                                                    (segment_tree2))))
                                    && (delta_dest == delta_sce))
                                    processNextDeltaAgain(param, xtarget,
                                                          ytarget,
                                                          delta_dest,
                                                          next_delta_sub_segment_list,
                                                          nb_of_sub_segments,
                                                          segment_dest);
                            }
                        }
                    }
                }
            }
            while ((next != RBTREENULL)
                   && (OFFSET(next) == segment_dest->P_MAX))
                next = getnextrbtree(next);
            segment_tree1 = next;
        }

        labeling_line++;
    }

}

/** **/

void labelPlane(ocrRoutingParameters * param, ocrWRoutingGrid * grid, ocrNaturalInt xtarget, ocrNaturalInt ytarget, ocrNaturalInt ztarget, ocrNaturalInt delta_sce,     // delta
                ocrNaturalInt plane_sce,        // z courant
                ocrNaturalInt plane_dest,       // new z
                chain_list ** next_delta_sub_segment_list,
                ocrNaturalInt * nb_of_sub_segments,
                chain_list ** labeled_segment_list)
{

    ocrNaturalInt nb_of_events = 2 * nb_of_sub_segments[plane_sce];
    ocrPlaneLabelingEvent *event_schedule;
    ocrNaturalInt schedule_index = 0;

    rbtree *labeling_status = RBTREENULL;

    ocrNaturalInt event_line;
    ocrNaturalInt labeling_line;

    ocrNaturalInt delta_dest;

    if (nb_of_events == 0)
        return;


    // cr�ation d'un �ch�ancier pour pouvoir faire un UNDO en cas de pbm
    event_schedule =
        makeSchedule(next_delta_sub_segment_list[plane_sce], nb_of_events);
    event_line = event_schedule->DATE;  // = date courante ?

    delta_dest = (((plane_dest > plane_sce) && (plane_dest <= ztarget)) ||
                  ((plane_dest < plane_sce) && (plane_dest >= ztarget))) ?
        delta_sce : delta_sce + 2 * param->VIA_COST;

    while (schedule_index < nb_of_events) {

        /* update */
        labeling_status = labelPlaneUpdate(labeling_status,
                                           event_schedule,
                                           &schedule_index,
                                           nb_of_events,
                                           &event_line, &labeling_line);

        /* process */
        labelPlaneProcess(param,
                          grid,
                          xtarget,
                          ytarget,
                          labeling_status,
                          event_line,
                          labeling_line,
                          delta_sce,
                          delta_dest,
                          plane_dest,
                          labeled_segment_list,
                          next_delta_sub_segment_list, nb_of_sub_segments);

    }

    mbkfree(event_schedule);
}

/** **/

void
initNextDeltaSubWSegList(ocrRoutingParameters * param,
                         ocrWRoutingGrid * grid,
                         ocrNaturalInt xtarget,
                         ocrNaturalInt ytarget,
                         chain_list *** list,
                         ocrNaturalInt ** nb_of_elements,
                         ocrWSegment * segment_source,
                         ocrNaturalInt xsource, ocrNaturalInt ysource)
{
    ocrNaturalInt i;

    *list =
        (chain_list **) mbkalloc(grid->NB_OF_LAYERS *
                                 sizeof(chain_list *));
    *nb_of_elements =
        (ocrNaturalInt *) mbkalloc(grid->NB_OF_LAYERS *
                                   sizeof(ocrNaturalInt));

    // Initialisation
    for (i = 0; i < grid->NB_OF_LAYERS; i++) {
        (*list)[i] = NULL;
        (*nb_of_elements)[i] = 0;
    }

    // Ajout du segment source
    (*nb_of_elements)[segment_source->LAYER] = 1;
    if (getWSegDirection(param, segment_source) == ocrHorizontal) {
        if (xsource < xtarget)
            (*list)[segment_source->LAYER] =
                addchain((*list)[segment_source->LAYER],
                         (void *) createSubWSegment(segment_source,
                                                    xsource,
                                                    MIN(segment_source->
                                                        P_MAX, xtarget)));
        else
            (*list)[segment_source->LAYER] =
                addchain((*list)[segment_source->LAYER],
                         (void *) createSubWSegment(segment_source,
                                                    MAX(segment_source->
                                                        P_MIN, xtarget),
                                                    xsource));
    } else                      // Vertical

    {
        if (ysource < ytarget)
            (*list)[segment_source->LAYER] =
                addchain((*list)[segment_source->LAYER],
                         (void *) createSubWSegment(segment_source,
                                                    ysource,
                                                    MIN(segment_source->
                                                        P_MAX, ytarget)));
        else
            (*list)[segment_source->LAYER] =
                addchain((*list)[segment_source->LAYER],
                         (void *) createSubWSegment(segment_source,
                                                    MAX(segment_source->
                                                        P_MIN, ytarget),
                                                    ysource));
    }
}

/** **/

void
initLabeledWSegList(ocrWRoutingGrid * grid,
                    chain_list *** list, ocrWSegment * segment_source)
{
    ocrNaturalInt i;

    *list =
        (chain_list **) mbkalloc(grid->NB_OF_LAYERS *
                                 sizeof(chain_list *));
    for (i = 0; i < grid->NB_OF_LAYERS; i++)
        (*list)[i] = NULL;

    (*list)[segment_source->LAYER] =
        addchain((*list)[segment_source->LAYER], (void *) segment_source);
}

/** **/

void
cleanLabeledWSegments(ocrWRoutingGrid * grid,
                      chain_list ** labeled_segment_list,
                      chain_list ** segment_garbage)
{
    ocrNaturalInt i;

    while (*segment_garbage) {
        unlabelWSegment((ocrWSegment *) (*segment_garbage)->DATA);
        *segment_garbage = delchain(*segment_garbage, *segment_garbage);
    }
    for (i = 0; i < grid->NB_OF_LAYERS; i++)
        while (labeled_segment_list[i]) {
            unlabelWSegment((ocrWSegment *) labeled_segment_list[i]->DATA);
            labeled_segment_list[i] =
                delchain(labeled_segment_list[i], labeled_segment_list[i]);
        }
}

int
isNextDeltaSubWSegListNotEmpty(ocrWRoutingGrid * grid, chain_list ** list)
{
    ocrNaturalInt i;

    for (i = 0; i < grid->NB_OF_LAYERS; i++)
        if (list[i])
            return 1;
    return 0;
}
