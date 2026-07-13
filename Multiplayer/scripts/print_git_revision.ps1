# STALKER-MP — emits the current git revision for build stamping.
#
# Usage (from a developer shell or build integration):
#   $rev = & Multiplayer\scripts\print_git_revision.ps1
#   msbuild ... /p:SmpGitRevision=$rev
# and pass /DSMP_GIT_REVISION="\"$rev\"" to the compiler, or add
#   <PreprocessorDefinitions>SMP_GIT_REVISION="$(SmpGitRevision)";...
# to a local property override. Unstamped builds report "unstamped"
# (see include/stalkermp/core/Version.h).

$ErrorActionPreference = "Stop"

$revision = git -C "$PSScriptRoot\.." rev-parse --short=12 HEAD 2>$null
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($revision)) {
    Write-Error "Not a git checkout or git unavailable."
    exit 1
}

$dirty = git -C "$PSScriptRoot\.." status --porcelain 2>$null
if (-not [string]::IsNullOrWhiteSpace($dirty)) {
    $revision = "$revision-dirty"
}

Write-Output $revision
