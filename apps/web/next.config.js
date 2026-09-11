/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  transpilePackages: ['@apex-telemetry/contracts', '@apex-telemetry/ui'],
  experimental: {}
};

module.exports = nextConfig;
