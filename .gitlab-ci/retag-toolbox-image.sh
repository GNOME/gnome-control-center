#!/bin/bash
set -e

die() {
  echo "$@" >&2
  exit 1
}

if [ "$BRANCH_NAME" == "main" ]; then
  SOURCE_IMAGE="${CI_REGISTRY_IMAGE}/${FDO_REPO_SUFFIX}:${FDO_DISTRIBUTION_TAG}"
  DEST_IMAGE="${CI_REGISTRY_IMAGE}/${FDO_REPO_SUFFIX}:toolbox"

  echo "Retagging ${SOURCE_IMAGE} as ${DEST_IMAGE}"
  podman tag "${SOURCE_IMAGE}" "${DEST_IMAGE}"
  podman push "${DEST_IMAGE}"
fi

