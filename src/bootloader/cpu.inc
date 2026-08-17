%ifndef CPU_INC
%define CPU_INC

%define CPUID_FEAT_EDX_FPU     (1 << 0)
%define CPUID_FEAT_EDX_MSR     (1 << 5)
%define CPUID_FEAT_EDX_PAE     (1 << 6)
%define CPUID_FEAT_EDX_APIC    (1 << 9)
%define CPUID_FEAT_EDX_MMX     (1 << 23)
%define CPUID_FEAT_EDX_FXSR    (1 << 24)
%define CPUID_FEAT_EDX_SSE     (1 << 25)
%define CPUID_FEAT_EDX_SSE2    (1 << 26)

%define CPUID_FEAT_ECX_SSE3    (1 << 0)
%define CPUID_FEAT_ECX_SSE4_1  (1 << 19)
%define CPUID_FEAT_ECX_SSE4_2  (1 << 20)
%define CPUID_FEAT_ECX_AVX     (1 << 28)
%define CPUID_FEAT_ECX_OSXSAVE (1 << 27)
%define CPUID_FEAT_ECX_XSAVE   (1 << 26)

%define CR0_MP  (1 << 1)
%define CR0_EM  (1 << 2)
%define CR0_NE  (1 << 5)
%define CR0_WP  (1 << 16)

%define CR4_OSFXSR      (1 << 9)
%define CR4_OSXMMEXCPT  (1 << 10)
%define CR4_OSXSAVE     (1 << 18)

%define XCR0_X87  (1 << 0)
%define XCR0_SSE  (1 << 1)
%define XCR0_AVX  (1 << 2)

%endif