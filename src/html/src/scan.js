if (!FEATURES.IS_TX)
    _('reset-model').addEventListener('click', postWithFeedback('Reset Model Settings', 'An error occurred resetting model settings', '/reset?model', null));

