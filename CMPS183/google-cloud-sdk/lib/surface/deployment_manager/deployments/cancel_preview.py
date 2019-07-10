# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""deployments cancel command."""

from apitools.base.py import exceptions as apitools_exceptions

from googlecloudsdk.api_lib.deployment_manager import dm_v2_util
from googlecloudsdk.calliope import base
from googlecloudsdk.calliope import exceptions
from googlecloudsdk.command_lib.deployment_manager import dm_base
from googlecloudsdk.core import log


# Number of seconds (approximately) to wait for cancel operation to complete.
OPERATION_TIMEOUT = 20 * 60  # 20 mins


class CancelPreview(base.Command, dm_base.DeploymentManagerCommand):
  """Cancel a pending or running deployment preview.

  This command will cancel a currently running or pending preview operation on
  a deployment.
  """

  detailed_help = {
      'DESCRIPTION': '{description}',
      'EXAMPLES': """\
          To cancel a running operation on a deployment, run:

            $ {command} my-deployment

          To issue a cancel preview command without waiting for the operation to complete, run:

            $ {command} my-deployment --async
          """,
  }

  @staticmethod
  def Args(parser):
    """Args is called by calliope to gather arguments for this command.

    Args:
      parser: An argparse parser that you can use to add arguments that go
          on the command line after this command. Positional arguments are
          allowed.
    """
    parser.add_argument(
        '--async',
        help='Return immediately and print information about the Operation in '
        'progress rather than waiting for the Operation to complete. '
        '(default=False)',
        dest='async',
        default=False,
        action='store_true')

    parser.add_argument('deployment_name', help='Deployment name.')

  def Run(self, args):
    """Run 'deployments cancel-preview'.

    Args:
      args: argparse.Namespace, The arguments that this command was invoked
          with.

    Returns:
      If --async=true, returns Operation to poll.
      Else, returns boolean indicating whether cancel preview operation
      succeeded.

    Raises:
      HttpException: An http error response was received while executing api
          request.
    """
    # Get the fingerprint from the previewing deployment to cancel.
    try:
      current_deployment = self.client.deployments.Get(
          self.messages.DeploymentmanagerDeploymentsGetRequest(
              project=self.project,
              deployment=args.deployment_name
          )
      )
      # If no fingerprint is present, default to an empty fingerprint.
      # This empty default can be removed once the fingerprint change is
      # fully implemented and all deployments have fingerprints.
      fingerprint = current_deployment.fingerprint or ''
    except apitools_exceptions.HttpError as error:
      raise exceptions.HttpException(error, dm_v2_util.HTTP_ERROR_FORMAT)

    try:
      operation = self.client.deployments.CancelPreview(
          self.messages.DeploymentmanagerDeploymentsCancelPreviewRequest(
              project=self.project,
              deployment=args.deployment_name,
              deploymentsCancelPreviewRequest=
              self.messages.DeploymentsCancelPreviewRequest(
                  fingerprint=fingerprint,
              ),
          )
      )
    except apitools_exceptions.HttpError as error:
      raise exceptions.HttpException(error, dm_v2_util.HTTP_ERROR_FORMAT)
    if args.async:
      return operation
    else:
      op_name = operation.name
      try:
        dm_v2_util.WaitForOperation(self.client,
                                    self.messages,
                                    op_name,
                                    self.project,
                                    'cancel-preview',
                                    OPERATION_TIMEOUT)
        log.status.Print('Cancel preview operation ' + op_name
                         + ' completed successfully.')
      except apitools_exceptions.HttpError as error:
        raise exceptions.HttpException(error, dm_v2_util.HTTP_ERROR_FORMAT)
      try:
        # Fetch a list of the canceled resources.
        response = self.client.resources.List(
            self.messages.DeploymentmanagerResourcesListRequest(
                project=self.project,
                deployment=args.deployment_name,
            )
        )
        # TODO(user): Pagination
        return response.resources if response.resources else []
      except apitools_exceptions.HttpError as error:
        raise exceptions.HttpException(error, dm_v2_util.HTTP_ERROR_FORMAT)
