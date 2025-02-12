import { Component, inject } from '@angular/core';
import { MatButton } from '@angular/material/button';
import { MAT_DIALOG_DATA, MatDialogActions, MatDialogClose, MatDialogContent, MatDialogRef, MatDialogTitle } from '@angular/material/dialog';
import { MatIcon } from '@angular/material/icon';

import { ReorderColumnsComponent } from '../components/reorder-columns/reorder-columns.component';
import { Column } from '../models';

@Component({
    template: `
        <h2 matDialogTitle>Reorder Columns</h2>

        <mat-dialog-content>
            <p class="instructions">
                Drag and drop by the <mat-icon>drag_handle</mat-icon> icon to
                reorder columns.
            </p>
            <app-reorder-columns [(columns)]="columns"></app-reorder-columns>
        </mat-dialog-content>
        <mat-dialog-actions>
            <button mat-button mat-dialog-close>Cancel</button>
            <button mat-raised-button cdkFocusInitial (click)="onSave()">
                Save
            </button>
        </mat-dialog-actions>
    `,
    styles: [
        `
            .instructions mat-icon {
                vertical-align: middle;
            }
        `,
    ],
    imports: [
        MatButton,
        MatDialogClose,
        MatDialogContent,
        MatDialogActions,
        MatDialogTitle,
        MatIcon,
        ReorderColumnsComponent,
    ],
    standalone: true,
})
export class ReorderColumnsDialogComponent {
    columns: Column[] = inject(MAT_DIALOG_DATA).columns as Column[];
    dialogRef = inject(MatDialogRef);

    onSave() {
        this.dialogRef.close(this.columns);
    }
}
