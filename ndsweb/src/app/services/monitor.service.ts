import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { APP_SERVER_URL } from '../tokens';

@Injectable()
export class MonitorService {
    serverUrl = inject(APP_SERVER_URL);
    http = inject(HttpClient);

    getCanvases() {
        return this.http.get<Canvas[]>(`${this.serverUrl}/canvases`);
    }

    deleteCanvas(canvasId: number) {
        return this.http.delete(`${this.serverUrl}/canvases/${canvasId}`)
    }

    deleteFeature(canvasId: number, featureId: number) {
        return this.http.delete(`${this.serverUrl}/canvases/${canvasId}/features/${featureId}`)
    }

    startCanvases(canvases: Canvas[]) {
        const canvasIds = canvases.map((c) => c.id);
        return this.http.post(`${this.serverUrl}/canvases/start`, {
            canvasIds,
        });
    }

    stopCanvases(canvases: Canvas[]) {
        const canvasIds = canvases.map((c) => c.id);
        return this.http.post(`${this.serverUrl}/canvases/stop`, {
            canvasIds,
        });
    }
}

export interface Canvas {
    currentEffectName: string;
    effectsManager: {
        currentEffectIndex: number;
        effects: Effect[];
        fps: number;
        type: string;
    };
    features: Feature[];
    fps: number;
    height: number;
    id: number;
    name: string;
    width: number;
}

export interface Feature {
    bytesPerSecond: number;
    channel: number;
    clientBufferCount: number;
    friendlyName: string;
    height: number;
    hostName: string;
    id: number;
    isConnected: boolean;
    lastClientResponse?: {
        brightness: number;
        bufferPos: number;
        bufferSize: number;
        currentClock: number;
        flashVersion?: number | string;
        fpsDrawing: number;
        newestPacket: number;
        oldestPacket: number;
        responseSize: number;
        sequenceNumber: number;
        watts: number;
        wifiSignal: number;
    };
    offsetX: number;
    offsetY: number;
    port: number;
    queueDepth: number;
    queueMaxSize: number;
    reconnectCount: number;
    redGreenSwap: boolean;
    reversed: boolean;
    timeOffset: number;
    type: string;
    width: number;
}

interface Effect {
    maxSpeed: number;
    name: string;
    newParticleProbability: number;
    particleFadeTime: number;
    particleHoldTime: number;
    particleIgnition: number;
    particlePreignitionTime: number;
    particleSize: number;
    type: string;

    brightness: number;
    density: number;
    dotSize: number;
    everyNthDot: number;
    ledColorPerSecond: number;
    ledScrollSpeed: number;
    mirrored: boolean;
    palette: {
        blend: boolean;
        colors: {
            b: number;
            g: number;
            r: number;
        }[];
    };
    rampedColor: false;
}
