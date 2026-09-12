# Preserved development history

The default branch is the maintained source. The tags below preserve earlier source or separate candidates at exact commits.
No archived candidate gains new build, package, or gameplay acceptance through this cleanup.

A tag is a fixed source snapshot. Existing tree URLs and `git clone --branch <name>` can select these tags.
For local inspection, use `git fetch origin --tags` followed by `git switch --detach refs/tags/<name>`.
To resume development, create a temporary branch from the tag. Do not move an archive tag.

| Tag | Preserved commit | Disposition |
|---|---|---|
| `codex/posix-setup-toolchain-copy` | [`be8ac1d03ee19d55394b5a5f2d9d1506edd56659`](https://github.com/Alexbeav/recomp-ui/tree/be8ac1d03ee19d55394b5a5f2d9d1506edd56659) | Contribution source preserved; upstream acceptance remains a separate record. |
| `codex/sbi-picker-upstream` | [`6b9f2344875ab6417b5df3b5d783d5d2392ef63d`](https://github.com/Alexbeav/recomp-ui/tree/6b9f2344875ab6417b5df3b5d783d5d2392ef63d) | Contribution source preserved; upstream acceptance remains a separate record. |

Recorded 2026-09-13. Existing version tags and releases remain unchanged.
