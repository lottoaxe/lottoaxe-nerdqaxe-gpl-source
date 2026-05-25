import { Component } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { ToastrService } from 'ngx-toastr';
import { LoadingService } from 'src/app/services/loading.service';

@Component({
  selector: 'app-diagnostics',
  templateUrl: './diagnostics.component.html',
  styleUrls: ['./diagnostics.component.scss'],
})
export class DiagnosticsComponent {
  diagnosticsData: any = null;
  showRawJson: boolean = false;

  constructor(
    private http: HttpClient,
    private toastr: ToastrService,
    private loadingService: LoadingService,
  ) {}

  exportDiagnostics(): void {
    this.http.get<any>('/api/lottoaxe/diagnostics')
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: (data) => {
          this.diagnosticsData = data;
          const json = JSON.stringify(data, null, 2);
          const blob = new Blob([json], { type: 'application/json' });
          const url = window.URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          const timestamp = new Date().toISOString().replace(/[:.]/g, '-').substring(0, 19);
          a.download = `lottoaxe-diagnostics-${timestamp}.json`;
          a.click();
          window.URL.revokeObjectURL(url);
          this.toastr.success('Diagnostics exported - share this with the devs for beta support');
        },
        error: () => this.toastr.error('Failed to fetch diagnostics'),
      });
  }

  copyToClipboard(): void {
    if (!this.diagnosticsData) {
      this.exportPreview();
      return;
    }
    const json = JSON.stringify(this.diagnosticsData, null, 2);
    navigator.clipboard.writeText(json).then(
      () => this.toastr.success('Diagnostics copied to clipboard'),
      () => this.toastr.error('Copy failed'),
    );
  }

  exportPreview(): void {
    this.http.get<any>('/api/lottoaxe/diagnostics')
      .subscribe({
        next: (data) => {
          this.diagnosticsData = data;
          this.showRawJson = true;
        },
        error: () => this.toastr.error('Failed to fetch diagnostics'),
      });
  }

  toggleRawJson(): void {
    if (!this.diagnosticsData) {
      this.exportPreview();
    } else {
      this.showRawJson = !this.showRawJson;
    }
  }

  get formattedJson(): string {
    return this.diagnosticsData ? JSON.stringify(this.diagnosticsData, null, 2) : '';
  }
}
