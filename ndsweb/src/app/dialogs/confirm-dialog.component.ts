import { NgClass, NgIf } from '@angular/common';
import { Component, inject } from '@angular/core';
import { MatButton } from '@angular/material/button';
import {
    MAT_DIALOG_DATA,
    MatDialogActions,
    MatDialogClose,
    MatDialogContent,
    MatDialogRef,
    MatDialogTitle,
} from '@angular/material/dialog';
import { MatIcon } from '@angular/material/icon';

@Component({
    template: `
        <h2 matDialogTitle>{{ title }}</h2>

        <mat-dialog-content>
            <p>
                {{ message }}
            </p>
        </mat-dialog-content>
        <mat-dialog-actions>
            <button [ngClass]="cancelClass" mat-button mat-dialog-close>
                <mat-icon *ngIf="cancelIcon as icon">{{ icon }}</mat-icon
                >{{ cancelText }}
            </button>
            <button
                [ngClass]="confirmClass"
                class="confirm-action"
                mat-raised-button
                cdkFocusInitial
                [mat-dialog-close]="true"
            >
                <mat-icon *ngIf="confirmIcon as icon">{{ icon }}</mat-icon>
                {{ confirmText }}
            </button>
        </mat-dialog-actions>
    `,
    styles: [``],
    imports: [
        MatButton,
        MatDialogClose,
        MatDialogContent,
        MatDialogActions,
        MatDialogTitle,
        MatIcon,
        NgClass,
        NgIf,
    ],
    standalone: true,
})
export class ConfirmDialogComponent {
    get title() {
        return this.data.title || 'Confirm Action';
    }

    get message() {
        return this.data.message || 'Are you certain you want to do this?';
    }

    get cancelText() {
        return this.data.cancelText || 'Back';
    }

    get cancelIcon() {
        return this.data.cancelIcon || null;
    }

    get cancelClass() {
        return this.data.cancelClass || null;
    }

    get confirmText() {
        return this.data.confirmText || 'Confirm';
    }

    get confirmIcon() {
        return this.data.confirmIcon || null;
    }

    get confirmClass() {
        return this.data.confirmClass || null;
    }

    data = inject(MAT_DIALOG_DATA);
    dialogRef = inject(MatDialogRef);
}
