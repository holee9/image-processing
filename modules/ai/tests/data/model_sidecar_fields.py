"""The sidecar every AI test model carries (QA-B-197, REQ-AI-008).

A model is refused at load time when its sidecar lacks the five fields REQ-AI-008 names, so every test model has
one. The values say what the fixtures are -- toy models for wiring tests -- and claim nothing about a real model.
"""
import json


def sidecar_text(model_id, labels=None, optional=None, version="0.0.1", note=None, omit=()):
    """The sidecar JSON text of a test model.

    labels:   list of body-part labels, or None for a model that has none (bone suppression)
    optional: dict of REQ-AI-010 fields to add (intended_use, ... published_date)
    omit:     names of required fields to leave out (for the tests that check a refusal)
    """
    d = {
        "model_id": model_id,
        "version": version,
        "pccp_scope": "none: toy model for wiring tests",
        "training_data_hash": "none: not trained",
        "validation_metrics": {"claims_accuracy": 0},
    }
    if note is not None:
        d["note"] = note
    if labels is not None:
        d["labels"] = labels
    if optional:
        d.update(optional)
    for k in omit:
        d.pop(k, None)
    return json.dumps(d, indent=2) + "\n"


def write(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")
