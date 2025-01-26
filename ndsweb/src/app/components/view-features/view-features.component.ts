import { NgFor } from '@angular/common';
import {
    ChangeDetectionStrategy,
    Component,
    input,
    output,
} from '@angular/core';
import { MatIconButton } from '@angular/material/button';
import { MatIcon } from '@angular/material/icon';
import {
    MatList,
    MatListItem,
    MatListItemMeta,
    MatListItemTitle,
} from '@angular/material/list';

import { Canvas, Feature } from 'src/app/services';

@Component({
    selector: 'app-view-features',
    templateUrl: `./view-features.component.html`,
    styleUrl: `./view-features.component.scss`,
    changeDetection: ChangeDetectionStrategy.OnPush,
    standalone: true,
    imports: [
        MatList,
        MatListItem,
        NgFor,
        MatIcon,
        MatIconButton,
        MatListItemMeta,
        MatListItemTitle,
    ],
})
export class ViewFeaturesComponent {
    get features() {
        return this.canvas()?.features;
    }

    canvas = input<Canvas | null>();

    deleteFeature = output<{ canvas: Canvas; feature: Feature }>();

    onDeleteFeature(feature: Feature) {
        const canvas = this.canvas() as Canvas;
        this.deleteFeature.emit({ canvas, feature });
    }
}
