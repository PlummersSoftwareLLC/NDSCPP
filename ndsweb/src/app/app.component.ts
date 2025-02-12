import { VERSION as ngVersion } from '@angular/common';
import { Component } from '@angular/core';
import { VERSION as ngMaterialVersion } from '@angular/material/core';
import { RouterModule } from '@angular/router';

import { environment } from 'src/environments/environment';

@Component({
    imports: [RouterModule],
    selector: 'app-root',
    templateUrl: './app.component.html',
    styleUrl: './app.component.scss',
})
export class AppComponent {
    constructor() {
        console.log('@angular/core', ngVersion.full);
        console.log('@angular/material', ngMaterialVersion.full);
        console.log(
            `Build ${environment.buildVersion}-${environment.buildCommit}`
        );
        console.log(`Build Date`, new Date(environment.buildDate as string));
    }
}
