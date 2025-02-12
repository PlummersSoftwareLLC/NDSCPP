import { Component, inject } from '@angular/core';
import { MatButton } from '@angular/material/button';
import { MatDialogActions, MatDialogClose, MatDialogContent, MatDialogTitle } from '@angular/material/dialog';

import { Store } from '@ngxs/store';

import { MonitorActions } from '../actions';
import { ViewFeaturesComponent } from '../components/view-features/view-features.component';
import { Canvas, Feature } from '../services';
import { MonitorState } from '../state';

@Component({
    template: `
        <h2 matDialogTitle>Canvas Features</h2>

        <mat-dialog-content>
            <app-view-features
                [canvas]="canvas()"
                (deleteFeature)="onDeleteFeature($event)"
            ></app-view-features>
        </mat-dialog-content>
        <mat-dialog-actions>
            <button mat-raised-button mat-dialog-close cdkFocusInitial>
                Close
            </button>
        </mat-dialog-actions>
    `,
    imports: [
        MatButton,
        MatDialogClose,
        MatDialogContent,
        MatDialogActions,
        MatDialogTitle,
        ViewFeaturesComponent,
    ],
    standalone: true,
})
export class ViewFeaturesDialogComponent {
    store = inject(Store);
    canvas = this.store.selectSignal(MonitorState.getSelectedCanvas());

    onDeleteFeature({ canvas, feature }: { canvas: Canvas; feature: Feature }) {
        this.store.dispatch(new MonitorActions.DeleteFeature(canvas, feature));
    }
}
