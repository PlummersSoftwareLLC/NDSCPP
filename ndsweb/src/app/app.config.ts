import { CommonModule } from '@angular/common';
import { provideHttpClient } from '@angular/common/http';
import { ApplicationConfig, provideZoneChangeDetection } from '@angular/core';
import { provideAnimationsAsync } from '@angular/platform-browser/animations/async';
import { provideRouter } from '@angular/router';

import { provideStore } from '@ngxs/store';

import { appRoutes } from './app.routes';
import { MonitorService } from './services';
import { MonitorState } from './state';
import { APP_SERVER_URL } from './tokens';
import { provideToastr } from 'ngx-toastr';

export const appConfig: ApplicationConfig = {
    providers: [
        CommonModule,
        provideHttpClient(),
        provideAnimationsAsync(),
        provideZoneChangeDetection({ eventCoalescing: true }),
        provideToastr({
            countDuplicates: true,
            preventDuplicates: true,
            resetTimeoutOnDuplicate: true,
            maxOpened: 5,
        }),
        provideRouter(appRoutes),
        provideStore([MonitorState], {
            developmentMode: true,
        }),

        MonitorService,

        { provide: APP_SERVER_URL, useValue: 'http://localhost:7777/api' },
    ],
};
