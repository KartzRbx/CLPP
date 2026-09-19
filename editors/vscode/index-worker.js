"use strict";

const { parentPort, workerData } = require("worker_threads");
const { indexMethods } = require("./workspace-index");

try {
  parentPort.postMessage(indexMethods((workerData && workerData.folders) || []));
} catch {
  parentPort.postMessage([]);
}
