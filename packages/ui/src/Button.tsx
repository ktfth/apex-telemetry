import React from 'react';

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'primary' | 'secondary' | 'outline' | 'ghost' | 'danger';
  size?: 'sm' | 'md' | 'xs';
}

export const Button: React.FC<ButtonProps> = ({
  children,
  variant = 'secondary',
  size = 'sm',
  className = '',
  disabled,
  ...props
}) => {
  const sizeClasses = {
    xs: 'px-2 py-1 text-[11px]',
    sm: 'px-2.5 py-1.5 text-xs',
    md: 'px-3 py-2 text-sm'
  }[size];

  const variantClasses = {
    primary: 'bg-neutral-100 text-neutral-900 hover:bg-neutral-200 border-neutral-100 font-semibold',
    secondary: 'bg-[#1a202c] text-neutral-200 hover:bg-[#222938] border-[#2d3748]',
    outline: 'bg-transparent text-neutral-300 hover:bg-[#181d26] border-[#232936]',
    ghost: 'bg-transparent text-neutral-400 hover:text-neutral-200 hover:bg-[#181d26] border-transparent',
    danger: 'bg-rose-950/40 text-rose-300 hover:bg-rose-900/60 border-rose-800'
  }[variant];

  return (
    <button
      className={`inline-flex items-center justify-center gap-1.5 font-mono border transition-colors select-none disabled:opacity-50 disabled:cursor-not-allowed ${sizeClasses} ${variantClasses} ${className}`}
      disabled={disabled}
      {...props}
    >
      {children}
    </button>
  );
};
