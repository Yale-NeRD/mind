#!/bin/bash
ssh sslee@vmhost1.cloud.cs.yale.internal -o "StrictHostKeyChecking no" -t "exit"
ssh sslee@vmhost2.cloud.cs.yale.internal -o "StrictHostKeyChecking no" -t "exit"
ssh sslee@vmhost3.cloud.cs.yale.internal -o "StrictHostKeyChecking no" -t "exit"
ssh sslee@vmhost5.cloud.cs.yale.internal -o "StrictHostKeyChecking no" -t "exit"
