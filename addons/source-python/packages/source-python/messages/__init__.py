# ../messages/__init__.py

"""Provides user message based functionality."""

# ============================================================================
# >> IMPORTS
# ============================================================================
# Source.Python Imports
#   Loggers
from loggers import _sp_logger
#   Messages
from messages.base import VGUIMenu
from messages.base import ShowMenu
from messages.base import SayText2
from messages.base import HintText
from messages.base import SayText
from messages.base import Shake
from messages.base import ResetHUD
from messages.base import TextMsg
from messages.base import KeyHintText
from messages.base import Fade
from messages.base import HudMsg
from messages.base import UserMessageCreator
from messages.base import UserMessage


# ============================================================================
# >> FORWARD IMPORTS
# ============================================================================
#   Messages
from _messages import DialogType


# =============================================================================
# >> ALL DECLARATION
# =============================================================================
__all__ = ('DialogType',
           'Fade',
           'HintText',
           'HudMsg',
           'KeyHintText',
           'ResetHUD',
           'SayText',
           'SayText2',
           'Shake',
           'ShowMenu',
           'TextMsg',
           'UserMessage',
           'UserMessageCreator',
           'VGUIMenu
           )


# =============================================================================
# >> GLOBAL VARIABLES
# =============================================================================
# Get the sp.messages logger
messages_logger = _sp_logger.messages
