import { HttpErrorResponse } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';

import { catchError, finalize, switchMap, take, tap, timer } from 'rxjs';

import {
    Action,
    createSelector,
    State,
    StateContext,
    Store,
} from '@ngxs/store';

import { MonitorActions } from '../actions';
import { Canvas, MonitorService } from '../services/monitor.service';
import { ToastrService } from 'ngx-toastr';
import { MatDialog } from '@angular/material/dialog';
import { ConfirmDialogComponent } from '../dialogs/confirm-dialog.component';
import { ViewFeaturesDialogComponent } from '../dialogs/view-features-dialog.component';

const REFRESH_INTERVAL_IN_MS = 80;

export interface StateModel {
    autoRefresh: boolean;
    isLoading: boolean;
    canvases: Canvas[];
    connectionError: HttpErrorResponse | null;
    selectedCanvas: Canvas | null;
    updateSelectedCanvas: boolean;
}

@State<StateModel>({
    name: 'monitor',
    defaults: {
        autoRefresh: true,
        isLoading: false,
        connectionError: null,
        canvases: [],
        selectedCanvas: null,
        updateSelectedCanvas: false,
    },
})
@Injectable()
export class MonitorState {
    static getCanvases() {
        return createSelector(
            [MonitorState],
            (state: StateModel) => state.canvases
        );
    }

    static autoRefresh() {
        return createSelector(
            [MonitorState],
            (state: StateModel) => state.autoRefresh
        );
    }

    static hasConnectionError() {
        return createSelector(
            [MonitorState],
            (state: StateModel) => state.connectionError !== null
        );
    }

    static connectionError() {
        return createSelector(
            [MonitorState],
            (state: StateModel) => state.connectionError
        );
    }

    static getSelectedCanvas() {
        return createSelector(
            [MonitorState],
            (state: StateModel) => state.selectedCanvas
        );
    }

    monitorService = inject(MonitorService);
    store = inject(Store);
    toastr = inject(ToastrService);
    dialog = inject(MatDialog);

    @Action(MonitorActions.UpdateAutoRefresh)
    updateAutoRefresh(
        { patchState }: StateContext<StateModel>,
        { value }: MonitorActions.UpdateAutoRefresh
    ) {
        patchState({ autoRefresh: value });

        if (value) {
            this.store.dispatch(new MonitorActions.LoadCanvases());
        }
    }

    @Action(MonitorActions.LoadCanvases)
    loadCanvases(
        { patchState, getState, dispatch }: StateContext<StateModel>,
        action: MonitorActions.LoadCanvases
    ) {
        const { isLoading } = getState();

        if (isLoading) {
            return;
        }

        patchState({ isLoading: true });

        return timer(REFRESH_INTERVAL_IN_MS).pipe(
            take(1),
            switchMap(() => this.monitorService.getCanvases()),
            tap((canvases) => {
                patchState({ canvases, connectionError: null });

                const { updateSelectedCanvas, selectedCanvas: current } =
                    getState();

                if (updateSelectedCanvas && current) {
                    const selectedCanvas = canvases.find(
                        (c) => c.id === current.id
                    );

                    patchState({ selectedCanvas, updateSelectedCanvas: false });
                }
            }),
            catchError((error) =>
                dispatch(new MonitorActions.LoadCanvasesFailure(error))
            ),
            finalize(() => {
                patchState({ isLoading: false });
                const { autoRefresh } = getState();

                if (autoRefresh) {
                    // Instead of firing off actions on a timer, we check if the user has toggled auto-refresh
                    // and if so, we dispatch a new action to load the controller data.
                    // This is a more reactive approach, and allows the user to control the refresh rate
                    // Also prevents multiple requests from being fired off to the server if the request
                    // takes longer than the timer interval
                    dispatch(new MonitorActions.LoadCanvases());
                }
            })
        );
    }

    @Action(MonitorActions.ViewFeatures)
    viewFeatures(
        { patchState }: StateContext<StateModel>,
        { canvas }: MonitorActions.ViewFeatures
    ) {
        patchState({ selectedCanvas: canvas });

        return this.dialog
            .open(ViewFeaturesDialogComponent)
            .afterClosed()
            .pipe(tap(() => patchState({ selectedCanvas: null })));
    }

    @Action(MonitorActions.StartCanvases)
    startCanvases(
        { dispatch }: StateContext<StateModel>,
        { canvases }: MonitorActions.StartCanvases
    ) {
        if (canvases.length === 0) {
            return;
        }

        return this.monitorService
            .startCanvases(canvases)
            .pipe(
                catchError((error) =>
                    dispatch(new MonitorActions.StartCanvasesFailure(error))
                )
            );
    }

    @Action(MonitorActions.StopCanvases)
    stopCanvases(
        { dispatch }: StateContext<StateModel>,
        { canvases }: MonitorActions.StopCanvases
    ) {
        if (canvases.length === 0) {
            return;
        }

        return this.monitorService
            .stopCanvases(canvases)
            .pipe(
                catchError((error) =>
                    dispatch(new MonitorActions.StopCanvasesFailure(error))
                )
            );
    }

    @Action(MonitorActions.DeleteCanvas)
    deleteCanvas(
        { dispatch }: StateContext<StateModel>,
        { canvas }: MonitorActions.DeleteCanvas
    ) {
        return this.monitorService.deleteCanvas(canvas.id).pipe(
            tap(() => {
                this.toastr.success('Canvas deleted successfully');
            }),
            catchError((error) =>
                dispatch(new MonitorActions.DeleteCanvasFailure(error))
            )
        );
    }

    @Action(MonitorActions.DeleteFeature)
    deleteFeature(
        { dispatch, patchState }: StateContext<StateModel>,
        { canvas, feature }: MonitorActions.DeleteFeature
    ) {
        return this.monitorService.deleteFeature(canvas.id, feature.id).pipe(
            tap(() => {
                this.toastr.success('Feature deleted successfully');

                patchState({ updateSelectedCanvas: true });
            }),
            catchError((error) =>
                dispatch(new MonitorActions.DeleteFeatureFailure(error))
            )
        );
    }

    @Action(MonitorActions.ConfirmDeleteCanvas)
    confirmDeleteCanvas(
        { dispatch }: StateContext<StateModel>,
        { canvas }: MonitorActions.ConfirmDeleteCanvas
    ) {
        return this.dialog
            .open(ConfirmDialogComponent, {
                disableClose: true,
                data: {
                    title: 'Confirm Delete',
                    message: `Are you certain you want to delete this canvas? This will remove (${canvas.features.length}) features.`,
                    cancelText: 'No',
                    confirmText: 'Yes',
                    confirmIcon: 'delete',
                    confirmClass: 'warn',
                },
            })
            .afterClosed()
            .pipe(
                tap((result) => {
                    if (result) {
                        dispatch(new MonitorActions.DeleteCanvas(canvas));
                    }
                })
            );
    }

    @Action(MonitorActions.ConfirmDeleteFeature)
    confirmDeleteFeature(
        { dispatch }: StateContext<StateModel>,
        { model }: MonitorActions.ConfirmDeleteFeature
    ) {
        return this.dialog
            .open(ConfirmDialogComponent, {
                disableClose: true,
                data: {
                    title: 'Confirm Delete',
                    message: 'Are you certain you want to delete this feature?',
                    cancelText: 'No',
                    confirmText: 'Yes',
                    confirmIcon: 'delete',
                    confirmClass: 'warn',
                },
            })
            .afterClosed()
            .pipe(
                tap((result) => {
                    if (result) {
                        dispatch(
                            new MonitorActions.DeleteFeature(
                                model.canvas,
                                model.feature
                            )
                        );
                    }
                })
            );
    }

    @Action([
        MonitorActions.LoadCanvasesFailure,
        MonitorActions.DeleteCanvasFailure,
        MonitorActions.DeleteFeatureFailure,
        MonitorActions.StartCanvasesFailure,
        MonitorActions.StopCanvasesFailure,
    ])
    handleError({ patchState }: StateContext<StateModel>, action: ErrorType) {
        switch (true) {
            case action instanceof MonitorActions.LoadCanvasesFailure:
                this.toastr.error(
                    action.error.message,
                    'Error loading canvases',
                    { disableTimeOut: true }
                );
                break;
            case action instanceof MonitorActions.StartCanvasesFailure:
                this.toastr.error(
                    action.error.message,
                    'Error activating canvas',
                    { disableTimeOut: true }
                );
                break;
            case action instanceof MonitorActions.StopCanvasesFailure:
                this.toastr.error(
                    action.error.message,
                    'Error deactivating canvas',
                    { disableTimeOut: true }
                );
                break;
            case action instanceof MonitorActions.DeleteCanvasFailure:
                this.toastr.error(
                    action.error.message,
                    'Error deleting canvas',
                    { disableTimeOut: true }
                );
                break;
            case action instanceof MonitorActions.DeleteFeatureFailure:
                this.toastr.error(
                    action.error.message,
                    'Error deleting feature',
                    { disableTimeOut: true }
                );
                break;
        }
    }
}

type ErrorType =
    | MonitorActions.LoadCanvasesFailure
    | MonitorActions.DeleteCanvasFailure
    | MonitorActions.DeleteFeatureFailure
    | MonitorActions.StartCanvasesFailure
    | MonitorActions.StopCanvasesFailure;
