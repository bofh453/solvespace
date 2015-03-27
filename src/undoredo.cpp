//-----------------------------------------------------------------------------
// The user-visible undo/redo operation; whenever they change something, we
// record our state and push it on a stack, and we pop the stack when they
// select undo.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpaceUI::UndoRemember(void) {
    unsaved = true;
    PushFromCurrentOnto(&undo);
    UndoClearStack(&redo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoUndo(void) {
    if(undo.cnt <= 0) return;

    PushFromCurrentOnto(&redo);
    PopOntoCurrentFrom(&undo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoRedo(void) {
    if(redo.cnt <= 0) return;

    PushFromCurrentOnto(&undo);
    PopOntoCurrentFrom(&redo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoEnableMenus(void) {
    EnableMenuById(GraphicsWindow::MNU_UNDO, undo.cnt > 0);
    EnableMenuById(GraphicsWindow::MNU_REDO, redo.cnt > 0);
}

void SolveSpaceUI::PushFromCurrentOnto(UndoStack *uk) {
    int i;

    if(uk->cnt == MAX_UNDO) {
        UndoClearState(&(uk->d[uk->write]));
        // And then write in to this one again
    } else {
        (uk->cnt)++;
    }

    UndoState *ut = &(uk->d[uk->write]);
    *ut = {};
    for(Group &src : SK.group) {
        Group dest = *src;
        // And then clean up all the stuff that needs to be a deep copy,
        // and zero out all the dynamic stuff that will get regenerated.
        dest.clean = false;
        dest.solved = {};
        dest.polyLoops = {};
        dest.bezierLoops = {};
        dest.bezierOpens = {};
        dest.polyError = {};
        dest.thisMesh = {};
        dest.runningMesh = {};
        dest.thisShell = {};
        dest.runningShell = {};
        dest.displayMesh = {};
        dest.displayEdges = {};

        dest.remap = {};
        src->remap.DeepCopyInto(&(dest.remap));

        dest.impMesh = {};
        dest.impShell = {};
        dest.impEntity = {};
        ut->group.Add(dest);
    }
    for(i = 0; i < SK.request.n; i++) {
        ut->request.Add((SK.request.elem[i]));
    }
    for(Constraint &src : SK.constraint) {
        Constraint dest = *src;
        dest.dogd = {};
        ut->constraint.Add(dest);
    }
    for(i = 0; i < SK.param.n; i++) {
        ut->param.Add((SK.param.elem[i]));
    }
    for(i = 0; i < SK.style.n; i++) {
        ut->style.Add((SK.style.elem[i]));
    }
    ut->activeGroup = SS.GW.activeGroup;

    uk->write = WRAP(uk->write + 1, MAX_UNDO);
}

void SolveSpaceUI::PopOntoCurrentFrom(UndoStack *uk) {
    if(uk->cnt <= 0) oops();
    (uk->cnt)--;
    uk->write = WRAP(uk->write - 1, MAX_UNDO);

    UndoState *ut = &(uk->d[uk->write]);

    // Free everything in the main copy of the program before replacing it
    for(Group *g : SK.group) {
        g->Clear();
    }
    SK.group.Clear();
    SK.request.Clear();
    SK.constraint.Clear();
    SK.param.Clear();
    SK.style.Clear();

    // And then do a shallow copy of the state from the undo list
    ut->group.MoveSelfInto(&(SK.group));
    ut->request.MoveSelfInto(&(SK.request));
    ut->constraint.MoveSelfInto(&(SK.constraint));
    ut->param.MoveSelfInto(&(SK.param));
    ut->style.MoveSelfInto(&(SK.style));
    SS.GW.activeGroup = ut->activeGroup;

    // No need to free it, since a shallow copy was made above
    *ut = {};

    // And reset the state everywhere else in the program, since the
    // sketch just changed a lot.
    SS.GW.ClearSuper();
    SS.TW.ClearSuper();
    SS.ReloadAllImported();
    SS.GenerateAll(0, INT_MAX);
    later.showTW = true;
}

void SolveSpaceUI::UndoClearStack(UndoStack *uk) {
    while(uk->cnt > 0) {
        uk->write = WRAP(uk->write - 1, MAX_UNDO);
        (uk->cnt)--;
        UndoClearState(&(uk->d[uk->write]));
    }
    *uk = {}; // for good measure
}

void SolveSpaceUI::UndoClearState(UndoState *ut) {
    int i;
    for(Group &g : ut->group) {

        g->remap.Clear();
    }
    ut->group.Clear();
    ut->request.Clear();
    ut->constraint.Clear();
    ut->param.Clear();
    ut->style.Clear();
    *ut = {};
}

