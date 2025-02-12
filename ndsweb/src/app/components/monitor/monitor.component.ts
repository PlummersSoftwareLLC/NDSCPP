import { SelectionModel } from '@angular/cdk/collections';
import { moveItemInArray } from '@angular/cdk/drag-drop';
import { CommonModule, NgTemplateOutlet } from '@angular/common';
import { Component, HostListener, inject, input, OnChanges, output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCheckboxChange, MatCheckboxModule } from '@angular/material/checkbox';
import { MatDialog } from '@angular/material/dialog';
import { MatFormField, MatLabel } from '@angular/material/form-field';
import { MatIcon } from '@angular/material/icon';
import { MatInput } from '@angular/material/input';
import { MatMenu, MatMenuItem, MatMenuTrigger } from '@angular/material/menu';
import { MatOption, MatSelect, MatSelectChange } from '@angular/material/select';
import { MatTableDataSource, MatTableModule } from '@angular/material/table';

import { filter, tap } from 'rxjs';

import { ReorderColumnsDialogComponent } from '../../dialogs/reorder-columns-dialog.component';
import { Column } from '../../models';
import { FormatDeltaPipe, FormatSizePipe } from '../../pipes';
import { Canvas, Feature } from '../../services';

interface UserOptions {
    filter: string;
    columns: Column[];
    displayedColumns: string[];
    sortColumn: string | null;
    sortDirection: 'asc' | 'desc' | '';
}

interface RowData {
    id: string;
    canvasId: number;
    featureId: number;
    isConnected: boolean;
    canvasName: string;
    featureName: string;
    hostName: string;
    size: string;
    reconnectCount: number | null;
    reconnectStatus: 'good' | 'warning' | 'danger' | '' | null;
    canvasFps: number | null;
    featureFps: number | null;
    fpsStatus: 'good' | 'warning' | '' | null;
    queueDepth: number | null;
    queueMaxSize: number | null;
    queueStatus: 'good' | 'warning' | 'danger' | '' | null;
    bufferSize: number | null;
    bufferPosition: number | null;
    bufferStatus: 'good' | 'warning' | 'danger' | '' | null;
    wifiSignal: string | null;
    wifiSignalStatus: 'good' | 'warning' | 'danger' | '' | null;
    bandwidth: number | null;
    currentTime: number | null;
    delta: number | null;
    deltaStatus: 'good' | 'warning' | 'danger' | '' | null;
    flashVersion: string | null;
    status: 'connected' | 'disconnected';
    currentEffectName: string;
    index: string;
}

const USER_SETTINGS_KEY = 'userOptions.v1';

@Component({
    selector: 'app-monitor',
    templateUrl: './monitor.component.html',
    styleUrls: ['./monitor.component.scss'],
    imports: [
        CommonModule,
        NgTemplateOutlet,
        MatTableModule,
        MatButtonModule,
        MatIcon,
        MatCheckboxModule,
        MatFormField,
        MatInput,
        MatSelect,
        MatMenuItem,
        MatMenuTrigger,
        MatMenu,
        MatLabel,
        MatOption,
        FormsModule,
        FormatSizePipe,
        FormatDeltaPipe,
    ],
    standalone: true,
})
export class MonitorComponent implements OnChanges {
    readonly QUEUE_MAX_SIZE = 25;

    filter = '';
    sortColumn: string | null = null;
    sortDirection: 'asc' | 'desc' | '' = '';

    selection = new SelectionModel<string>(true, []);
    dataSource = new MatTableDataSource([] as RowData[]);

    canvases = input<Canvas[]>([]);
    autoRefresh = output<boolean>();
    startCanvases = output<Canvas[]>();
    stopCanvases = output<Canvas[]>();
    deleteCanvas = output<Canvas>();
    viewFeatures = output<Canvas>();

    dialog = inject(MatDialog);

    _displayedColumns = [
        'select',
        'canvasName',
        'featureName',
        'host',
        'size',
        'reconnectCount',
        'fps',
        'queueDepth',
        'buffer',
        'signal',
        'dataRate',
        'delta',
        'flash',
        'status',
        'effect',
        'actions',
    ];

    columns: { key: string; value: string }[] = [
        { key: 'Canvas', value: 'canvasName' },
        { key: 'Feature', value: 'featureName' },
        { key: 'Host', value: 'host' },
        { key: 'Size', value: 'size' },
        { key: 'Cx', value: 'reconnectCount' },
        { key: 'FPS', value: 'fps' },
        { key: 'Queue', value: 'queueDepth' },
        { key: 'Buffer', value: 'buffer' },
        { key: 'Signal', value: 'signal' },
        { key: 'Data', value: 'dataRate' },
        { key: 'Delta', value: 'delta' },
        { key: 'Flash', value: 'flash' },
        { key: 'Status', value: 'status' },
        { key: 'Effect', value: 'effect' },
    ];

    get displayedColumns(): string[] {
        return this._displayedColumns;
    }

    set displayedColumns(value: string[]) {
        this._displayedColumns = value;
    }

    onApplyFilter() {
        this.dataSource.filter = this.filter.toLocaleLowerCase();
        this.saveSettingsToStorage();
    }

    onSort(column: string) {
        if (this.sortColumn === column) {
            switch (this.sortDirection) {
                case 'asc':
                    this.sortDirection = 'desc';
                    break;
                case 'desc':
                    this.sortColumn = null;
                    this.sortDirection = '';
                    break;
                default:
                    this.sortDirection = 'asc';
                    break;
            }
        } else {
            this.sortColumn = column;
            this.sortDirection = 'asc';
        }

        this.saveSettingsToStorage();
    }

    onStartCanvases(rowIds: string[]) {
        const canvasIds = this.dataSource.filteredData
            .filter((r) => rowIds.includes(r.id))
            .map((r) => r.canvasId);

        const canvases = this.canvases().filter((d) =>
            canvasIds.includes(d.id)
        );

        if (canvases.length === 0) {
            return;
        }

        this.startCanvases.emit(canvases);
    }

    onStopCanvases(rowIds: string[]) {
        const canvasIds = this.dataSource.filteredData
            .filter((r) => rowIds.includes(r.id))
            .map((r) => r.canvasId);

        const canvases = this.canvases().filter((d) =>
            canvasIds.includes(d.id)
        );

        if (canvases.length === 0) {
            return;
        }

        this.stopCanvases.emit(canvases);
    }

    onSelectAllChange(event: MatCheckboxChange): void {
        this.dataSource.filteredData.forEach((r) => {
            if (event.checked) {
                this.selection.select(r.id);
            } else {
                this.selection.deselect(r.id);
            }
        });
    }

    areAllSelected(): boolean {
        return (
            this.dataSource.filteredData.length > 0 &&
            this.dataSource.filteredData.every((r) =>
                this.selection.isSelected(r.id)
            )
        );
    }

    areAnySelected(): boolean {
        return (
            this.dataSource.filteredData.some((r) =>
                this.selection.isSelected(r.id)
            ) && !this.areAllSelected()
        );
    }

    onClearFilter(): void {
        this.filter = '';
        this.dataSource.filter = '';
    }

    updateFilter(filter: string | null) {
        this.filter = filter || '';
        this.dataSource.filter = filter || '';
    }

    onViewFeatures(row: RowData) {
        const canvas = this.canvases().find((c) => c.id === row.canvasId);

        if (canvas) {
            this.viewFeatures.emit(canvas);
        }
    }

    trackByFn(index: number, item: RowData) {
        return item.id;
    }

    displayedColumnsSelectionChange(event: MatSelectChange) {
        this.updateDisplayedColumns(event.value);
        this.saveSettingsToStorage();
    }

    updateDisplayedColumns(columnsToDisplay: string[]) {
        this._displayedColumns = ['select']
            .concat(
                this.columns
                    .filter((c) => columnsToDisplay.includes(c.value))
                    .map((c) => c.value)
            )
            .concat(['actions']);
    }

    loadFilterFromStorage() {
        const filterOptions = localStorage.getItem(USER_SETTINGS_KEY);
        if (filterOptions) {
            try {
                const options = JSON.parse(filterOptions) as UserOptions;
                const columns = options.columns;
                const displayedColumns = options.displayedColumns;

                this.filter = options.filter || '';
                this.sortColumn = options.sortColumn || null;
                this.sortDirection = options.sortDirection || '';

                // only set the columns if they are valid
                this.setColumnSortOrder(columns);
                this.displayedColumns =
                    this.normalizeDisplayColumns(displayedColumns);
            } catch (e) {}
        }
    }

    setColumnSortOrder(columns: Column[]) {
        columns.reduceRight((acc, column) => {
            const existingIx = this.columns.findIndex(
                (c) => c.value === column.value
            );

            if (existingIx !== -1) {
                moveItemInArray(this.columns, existingIx, 0);
            }

            return acc;
        }, []);
    }

    normalizeDisplayColumns(columnsToDisplay: string[]): string[] {
        const columns = this.columns.reduce((acc, column) => {
            if (columnsToDisplay.includes(column.value)) {
                acc.push(column.value);
            }

            return acc;
        }, [] as string[]);

        if (columns.length) {
            return [
                'select',
                ...columns.filter((c) => c !== 'select' && c !== 'actions'),
                'actions',
            ];
        }

        return ['select', ...this.columns.map((c) => c.value), 'actions'];
    }

    saveSettingsToStorage() {
        localStorage.setItem(
            USER_SETTINGS_KEY,
            JSON.stringify({
                columns: this.columns,
                displayedColumns: this._displayedColumns,
                filter: this.filter,
                sortColumn: this.sortColumn,
                sortDirection: this.sortDirection,
            })
        );
    }

    transformFeatureToRowData(canvas: Canvas, feature: Feature) {
        const now = new Date().getTime();
        const isConnected = feature.isConnected;
        const reconnectCount =
            feature.reconnectCount !== undefined
                ? feature.reconnectCount
                : null;

        const data: RowData = {
            isConnected,
            id: `${canvas.id}-${feature.id}`,
            canvasId: canvas.id,
            featureId: feature.id,
            canvasName: canvas.name,
            featureName: feature.friendlyName,
            hostName: feature.hostName,
            size: `${feature.height}x${feature.width}`,
            reconnectCount: reconnectCount,
            reconnectStatus: !reconnectCount
                ? ''
                : reconnectCount < 3
                ? 'good'
                : reconnectCount < 10
                ? 'warning'
                : 'danger',
            canvasFps: canvas.fps,
            featureFps: 90,
            fpsStatus: null,
            bufferSize: null,
            bufferPosition: null,
            wifiSignal: null,
            bandwidth: null,
            currentTime: null,
            flashVersion: null,
            status: 'disconnected',
            currentEffectName: canvas.currentEffectName,
        } as RowData;

        if (isConnected && feature.lastClientResponse) {
            const { lastClientResponse } = feature;

            const fps = lastClientResponse.fpsDrawing;
            const bufferPos = lastClientResponse.bufferPos;
            const bufferSize = lastClientResponse.bufferSize;
            const signal = Math.abs(lastClientResponse.wifiSignal);
            const bytesPerSecond = feature.bytesPerSecond;
            const currentTime = lastClientResponse.currentClock;
            const queueDepth = feature.queueDepth;
            const queueMaxSize = feature.queueMaxSize;
            const flashVersion =
                lastClientResponse.flashVersion !== undefined
                    ? lastClientResponse.flashVersion.toString()
                    : null;
            const ratio = bufferPos / bufferSize;

            data.featureFps = fps;
            data.bufferSize = bufferSize;
            data.bufferPosition = bufferPos;
            data.wifiSignal = signal >= 100 ? 'LAN' : `${~~signal} dBm`;
            data.bandwidth = bytesPerSecond;
            data.currentTime = currentTime;
            data.delta = currentTime - now;
            data.flashVersion = flashVersion;
            data.status = 'connected';
            data.queueDepth = queueDepth;
            data.queueMaxSize = queueMaxSize;
            data.queueStatus =
                queueDepth < 100
                    ? 'good'
                    : queueDepth < 250
                    ? 'warning'
                    : 'danger';

            data.fpsStatus = fps < 0.8 * canvas.fps ? 'warning' : 'good';
            data.bufferStatus =
                ratio >= 0.25 && ratio <= 0.85
                    ? 'good'
                    : ratio > 0.95
                    ? 'warning'
                    : 'danger';

            data.wifiSignalStatus =
                signal >= 100
                    ? ''
                    : signal < 70
                    ? 'good'
                    : signal < 80
                    ? 'warning'
                    : 'danger';

            data.deltaStatus =
                Math.abs(data.delta) > 100
                    ? ''
                    : Math.abs(data.delta) < 2.0
                    ? 'good'
                    : Math.abs(data.delta) < 3.0
                    ? 'warning'
                    : 'danger';
        }

        const index = [
            (data.hostName || '').toLocaleLowerCase(),
            (data.featureName || '').toLocaleLowerCase(),
            (data.canvasName || '').toLocaleLowerCase(),
            (data.currentEffectName || '').toLocaleLowerCase(),
        ];

        data.index = index.join(';');

        return data;
    }

    // listen for the control key and refresh the data when it is pressed
    @HostListener('document:keydown.control')
    onControlDown() {
        this.autoRefresh.emit(false);
    }

    @HostListener('document:keyup.control')
    onControlUp() {
        this.autoRefresh.emit(true);
    }

    openReorderColumnsDialog() {
        this.dialog
            .open(ReorderColumnsDialogComponent, {
                disableClose: true,
                data: {
                    columns: this.columns.slice(0), // clone the columns
                },
            })
            .afterClosed()
            .pipe(
                tap((columns: Column[] | undefined) => {
                    if (columns) {
                        this.columns = columns;
                        this.updateDisplayedColumns(
                            this.columns.map((c) => c.value)
                        );
                        this.saveSettingsToStorage();
                    }
                })
            )
            .subscribe();
    }

    update() {
        const data = this.canvases().reduce((acc, canvas) => {
            const features = canvas.features.map((feature) =>
                this.transformFeatureToRowData(canvas, feature)
            );

            acc.push(...features);

            return acc;
        }, [] as RowData[]);

        if (this.sortColumn) {
            data.sort((a: any, b: any) => {
                if (!this.sortColumn) {
                    return 0;
                }
                const aVal = (a[this.sortColumn] || '')
                    .toString()
                    .toLocaleLowerCase();
                const bVal = (b[this.sortColumn] || '')
                    .toString()
                    .toLocaleLowerCase();

                if (aVal === bVal) {
                    return 0;
                }

                return this.sortDirection === 'asc'
                    ? aVal.localeCompare(bVal)
                    : bVal.localeCompare(aVal);
            });
        }

        this.dataSource = new MatTableDataSource(data);

        this.dataSource.filterPredicate = this.onFilterRowData.bind(this);
        this.dataSource.filter = this.filter + '.';
    }

    onDeleteCanvas(row: RowData) {
        const canvas = this.canvases().find((c) => c.id === row.canvasId);

        if (canvas) {
            this.deleteCanvas.emit(canvas);
        }
    }

    onFilterRowData(data: RowData, filter: string): boolean {
        return data.index.includes(this.filter.toLocaleLowerCase());
    }

    ngOnChanges(changes: SimpleChanges): void {
        const canvases = changes['canvases'];
        if (canvases) {
            if (canvases.isFirstChange()) {
                this.loadFilterFromStorage();
            }

            this.update();
        }
    }
}
