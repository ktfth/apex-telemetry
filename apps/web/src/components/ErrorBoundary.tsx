'use client';

import React, { Component, type ErrorInfo, type ReactNode } from 'react';

interface Props {
  children: ReactNode;
  fallback?: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
}

/**
 * Global error boundary that catches unhandled exceptions in the React tree.
 * Displays a minimal, dark-themed recovery screen consistent with the
 * ApexTelemetry instrumentation aesthetic.
 */
export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = { hasError: false, error: null };
  }

  static getDerivedStateFromError(error: Error): State {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error('[ApexTelemetry] Uncaught error:', error, info.componentStack);
  }

  handleReload = () => {
    this.setState({ hasError: false, error: null });
    window.location.reload();
  };

  render() {
    if (this.state.hasError) {
      if (this.props.fallback) return this.props.fallback;

      return (
        <div className="flex items-center justify-center h-screen w-screen bg-[#0a0c10]">
          <div className="max-w-md text-center space-y-4 p-8">
            <div className="text-red-400 font-mono text-xs tracking-widest uppercase">
              ⚠ Runtime Exception
            </div>
            <h1 className="text-xl font-semibold text-neutral-200">
              Apex<span className="text-sky-400">Telemetry</span> encountered an error
            </h1>
            <p className="text-sm text-neutral-400 font-mono break-all">
              {this.state.error?.message || 'Unknown error'}
            </p>
            <button
              onClick={this.handleReload}
              className="mt-4 px-4 py-2 bg-sky-600 hover:bg-sky-500 text-white text-sm font-medium rounded transition-colors"
            >
              Reload Dashboard
            </button>
          </div>
        </div>
      );
    }

    return this.props.children;
  }
}
