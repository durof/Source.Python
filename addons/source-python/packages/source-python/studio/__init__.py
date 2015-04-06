# ../studio/__init__.py

"""Provides model based functionality."""

# =============================================================================
# >> IMPORTS
# =============================================================================
# Source.Python Imports
#   Loggers
from loggers import _sp_logger


# =============================================================================
# >> ALL DECLARATION
# =============================================================================
__all__ = ('studio_logger',
           )


# =============================================================================
# >> GLOBAL VARIABLES
# =============================================================================
# Get the sp.listeners logger
studio_logger = _sp_logger.studio
