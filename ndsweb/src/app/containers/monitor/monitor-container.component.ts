import { CommonModule } from '@angular/common';
import { ChangeDetectionStrategy, Component } from '@angular/core';
import { MatCardModule } from '@angular/material/card';
import { MatIcon } from '@angular/material/icon';

import { Store } from '@ngxs/store';

import { MonitorActions } from '../../actions';
import { MonitorComponent } from '../../components';
import { Canvas, Feature } from '../../services';
import { MonitorState } from '../../state';

@Component({
    templateUrl: './monitor-container.component.html',
    styleUrls: ['./monitor-container.component.scss'],
    changeDetection: ChangeDetectionStrategy.OnPush,
    imports: [MonitorComponent, MatCardModule, MatIcon, CommonModule],
    standalone: true,
})
export class MonitorContainerComponent {
    canvases = this.store.selectSignal(MonitorState.getCanvases());
    connectionError = this.store.selectSignal(MonitorState.connectionError());

    constructor(private store: Store) {}

    onAutoRefresh(value: boolean) {
        this.store.dispatch(new MonitorActions.UpdateAutoRefresh(value));
    }

    onActivateCanvases(canvases: Canvas[]) {
        this.store.dispatch(new MonitorActions.ActivateCanvases(canvases));
    }

    onDeactivateCanvases(canvases: Canvas[]) {
        this.store.dispatch(new MonitorActions.DeactivateCanvases(canvases));
    }

    onDeleteCanvas(canvas: Canvas) {
        this.store.dispatch(new MonitorActions.ConfirmDeleteCanvas(canvas));
    }

    onDeleteFeature(model: { canvas: Canvas; feature: Feature }) {
        this.store.dispatch(new MonitorActions.ConfirmDeleteFeature(model));
    }
}