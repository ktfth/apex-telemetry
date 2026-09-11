import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'ApexTelemetry — F1 High-Precision Engineering Telemetry',
  description: 'Análise aprofundada e sóbria de telemetria de Fórmula 1 alinhada por distância espacial com evidências auditáveis.',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="pt-BR" className="dark h-full">
      <body className="h-full overflow-hidden bg-[#0a0c10] text-[#f3f4f6] font-sans antialiased">
        {children}
      </body>
    </html>
  );
}
